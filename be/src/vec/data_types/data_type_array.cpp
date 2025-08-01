// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
// This file is copied from
// https://github.com/ClickHouse/ClickHouse/blob/master/src/DataTypes/DataTypeArray.h
// and modified by Doris

#include "vec/data_types/data_type_array.h"

#include <ctype.h>
#include <gen_cpp/data.pb.h>
#include <glog/logging.h>
#include <string.h>

#include <typeinfo>
#include <utility>

#include "agent/be_exec_version_manager.h"
#include "runtime/decimalv2_value.h"
#include "util/types.h"
#include "vec/columns/column.h"
#include "vec/columns/column_array.h"
#include "vec/columns/column_const.h"
#include "vec/columns/column_nullable.h"
#include "vec/common/assert_cast.h"
#include "vec/common/string_buffer.hpp"
#include "vec/common/string_ref.h"
#include "vec/common/typeid_cast.h"
#include "vec/data_types/data_type_nullable.h"
#include "vec/io/reader_buffer.h"

namespace doris::vectorized {

namespace ErrorCodes {
extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
}

DataTypeArray::DataTypeArray(const DataTypePtr& nested_) : nested {nested_} {}

MutableColumnPtr DataTypeArray::create_column() const {
    return ColumnArray::create(nested->create_column(), ColumnArray::ColumnOffsets::create());
}

Status DataTypeArray::check_column(const IColumn& column) const {
    const auto* column_array = DORIS_TRY(check_column_nested_type<ColumnArray>(column));
    return nested->check_column(column_array->get_data());
}

Field DataTypeArray::get_default() const {
    Array a;
    a.push_back(nested->get_default());
    return Field::create_field<TYPE_ARRAY>(a);
}

bool DataTypeArray::equals(const IDataType& rhs) const {
    return typeid(rhs) == typeid(*this) &&
           nested->equals(*static_cast<const DataTypeArray&>(rhs).nested);
}

// here we should remove nullable, otherwise here always be 1
size_t DataTypeArray::get_number_of_dimensions() const {
    const DataTypeArray* nested_array =
            typeid_cast<const DataTypeArray*>(remove_nullable(nested).get());
    if (!nested_array) return 1;
    return 1 +
           nested_array
                   ->get_number_of_dimensions(); /// Every modern C++ compiler optimizes tail recursion.
}
// binary : const flag | row num | real saved num | offsets | data
// offsets: data_off1 | data_off2 | ...
// data   : data1 | data2 | ...
int64_t DataTypeArray::get_uncompressed_serialized_bytes(const IColumn& column,
                                                         int be_exec_version) const {
    if (be_exec_version >= USE_CONST_SERDE) {
        auto size = sizeof(bool) + sizeof(size_t) + sizeof(size_t);
        bool is_const_column = is_column_const(column);
        auto real_need_copy_num = is_const_column ? 1 : column.size();
        const IColumn* array_column = &column;
        if (is_const_column) {
            const auto& const_column = assert_cast<const ColumnConst&>(column);
            array_column = &(const_column.get_data_column());
        }
        const auto& data_column = assert_cast<const ColumnArray&>(*array_column);
        size = size + sizeof(ColumnArray::Offset64) * real_need_copy_num;
        return size + get_nested_type()->get_uncompressed_serialized_bytes(data_column.get_data(),
                                                                           be_exec_version);
    } else {
        auto ptr = column.convert_to_full_column_if_const();
        const auto& data_column = assert_cast<const ColumnArray&>(*ptr.get());
        return sizeof(ColumnArray::Offset64) * (column.size() + 1) +
               get_nested_type()->get_uncompressed_serialized_bytes(data_column.get_data(),
                                                                    be_exec_version);
    }
}

char* DataTypeArray::serialize(const IColumn& column, char* buf, int be_exec_version) const {
    if (be_exec_version >= USE_CONST_SERDE) {
        const auto* array_column = &column;
        size_t real_need_copy_num = 0;
        buf = serialize_const_flag_and_row_num(&array_column, buf, &real_need_copy_num);

        const auto& data_column = assert_cast<const ColumnArray&>(*array_column);
        // offsets
        memcpy(buf, data_column.get_offsets().data(),
               real_need_copy_num * sizeof(ColumnArray::Offset64));
        buf += real_need_copy_num * sizeof(ColumnArray::Offset64);
        // children
        return get_nested_type()->serialize(data_column.get_data(), buf, be_exec_version);
    } else {
        auto ptr = column.convert_to_full_column_if_const();
        const auto& data_column = assert_cast<const ColumnArray&>(*ptr.get());

        // row num
        unaligned_store<ColumnArray::Offset64>(buf, column.size());
        buf += sizeof(ColumnArray::Offset64);
        // offsets
        memcpy(buf, data_column.get_offsets().data(),
               column.size() * sizeof(ColumnArray::Offset64));
        buf += column.size() * sizeof(ColumnArray::Offset64);
        // children
        return get_nested_type()->serialize(data_column.get_data(), buf, be_exec_version);
    }
}

const char* DataTypeArray::deserialize(const char* buf, MutableColumnPtr* column,
                                       int be_exec_version) const {
    if (be_exec_version >= USE_CONST_SERDE) {
        auto* origin_column = column->get();
        size_t real_have_saved_num = 0;
        buf = deserialize_const_flag_and_row_num(buf, column, &real_have_saved_num);

        auto* data_column = assert_cast<ColumnArray*>(origin_column);
        auto& offsets = data_column->get_offsets();

        // offsets
        offsets.resize(real_have_saved_num);
        memcpy(offsets.data(), buf, sizeof(ColumnArray::Offset64) * real_have_saved_num);
        buf += sizeof(ColumnArray::Offset64) * real_have_saved_num;
        // children
        auto nested_column = data_column->get_data_ptr()->assume_mutable();
        buf = get_nested_type()->deserialize(buf, &nested_column, be_exec_version);
        return buf;
    } else {
        auto* data_column = assert_cast<ColumnArray*>(column->get());
        auto& offsets = data_column->get_offsets();

        // row num
        auto row_num = unaligned_load<ColumnArray::Offset64>(buf);
        buf += sizeof(ColumnArray::Offset64);
        // offsets
        offsets.resize(row_num);
        memcpy(offsets.data(), buf, sizeof(ColumnArray::Offset64) * row_num);
        buf += sizeof(ColumnArray::Offset64) * row_num;
        // children
        auto nested_column = data_column->get_data_ptr()->assume_mutable();
        return get_nested_type()->deserialize(buf, &nested_column, be_exec_version);
    }
}

void DataTypeArray::to_pb_column_meta(PColumnMeta* col_meta) const {
    IDataType::to_pb_column_meta(col_meta);
    auto children = col_meta->add_children();
    get_nested_type()->to_pb_column_meta(children);
}

void DataTypeArray::to_string(const IColumn& column, size_t row_num, BufferWritable& ostr) const {
    auto result = check_column_const_set_readability(column, row_num);
    ColumnPtr ptr = result.first;
    row_num = result.second;

    auto& data_column = assert_cast<const ColumnArray&>(*ptr);
    auto& offsets = data_column.get_offsets();

    size_t offset = offsets[row_num - 1];
    size_t next_offset = offsets[row_num];

    const IColumn& nested_column = data_column.get_data();
    ostr.write("[", 1);
    for (size_t i = offset; i < next_offset; ++i) {
        if (i != offset) {
            ostr.write(", ", 2);
        }
        if (is_string_type(nested->get_primitive_type())) {
            ostr.write("'", 1);
            nested->to_string(nested_column, i, ostr);
            ostr.write("'", 1);
        } else {
            nested->to_string(nested_column, i, ostr);
        }
    }
    ostr.write("]", 1);
}

std::string DataTypeArray::to_string(const IColumn& column, size_t row_num) const {
    auto result = check_column_const_set_readability(column, row_num);
    ColumnPtr ptr = result.first;
    row_num = result.second;

    auto& data_column = assert_cast<const ColumnArray&>(*ptr);
    auto& offsets = data_column.get_offsets();

    size_t offset = offsets[row_num - 1];
    size_t next_offset = offsets[row_num];
    const IColumn& nested_column = data_column.get_data();
    std::string str;
    str += "[";
    for (size_t i = offset; i < next_offset; ++i) {
        if (i != offset) {
            str += ", ";
        }
        if (is_string_type(nested->get_primitive_type())) {
            str += "'";
            str += nested->to_string(nested_column, i);
            str += "'";
        } else {
            str += nested->to_string(nested_column, i);
        }
    }
    str += "]";
    return str;
}

bool next_element_from_string(ReadBuffer& rb, StringRef& output, bool& has_quota) {
    StringRef element(rb.position(), 0);
    has_quota = false;
    if (rb.eof()) {
        return false;
    }

    // ltrim
    while (!rb.eof() && isspace(*rb.position())) {
        ++rb.position();
        element.data = rb.position();
    }

    // parse string
    if (*rb.position() == '"' || *rb.position() == '\'') {
        const char str_sep = *rb.position();
        size_t str_len = 1;
        // search until next '"' or '\''
        while (str_len < rb.count() && *(rb.position() + str_len) != str_sep) {
            ++str_len;
        }
        // invalid string
        if (str_len >= rb.count()) {
            rb.position() = rb.end();
            return false;
        }
        has_quota = true;
        rb.position() += str_len + 1;
        element.size += str_len + 1;
    }

    // parse array element until array separator ',' or end ']'
    while (!rb.eof() && (*rb.position() != ',') && (rb.count() != 1 || *rb.position() != ']')) {
        // invalid elements such as ["123" 456,"789" 777]
        // correct elements such as ["123"    ,"789"    ]
        if (has_quota && !isspace(*rb.position())) {
            return false;
        }
        ++rb.position();
        ++element.size;
    }
    // invalid array element
    if (rb.eof()) {
        return false;
    }
    // adjust read buffer position to first char of next array element
    ++rb.position();

    // rtrim
    while (element.size > 0 && isspace(element.data[element.size - 1])) {
        --element.size;
    }

    // trim '"' and '\'' for string
    if (element.size >= 2 && (element.data[0] == '"' || element.data[0] == '\'') &&
        element.data[0] == element.data[element.size - 1]) {
        ++element.data;
        element.size -= 2;
    }
    output = element;
    return true;
}

Status DataTypeArray::from_string(ReadBuffer& rb, IColumn* column) const {
    DCHECK(!rb.eof());
    // only support one level now
    auto* array_column = assert_cast<ColumnArray*>(column);
    auto& offsets = array_column->get_offsets();

    IColumn& nested_column = array_column->get_data();
    DCHECK(nested_column.is_nullable());
    if (*rb.position() != '[') {
        return Status::InvalidArgument("Array does not start with '[' character, found '{}'",
                                       *rb.position());
    }
    if (*(rb.end() - 1) != ']') {
        return Status::InvalidArgument("Array does not end with ']' character, found '{}'",
                                       *(rb.end() - 1));
    }
    // empty array []
    if (rb.count() == 2) {
        offsets.push_back(offsets.back());
        return Status::OK();
    }
    ++rb.position();

    size_t element_num = 0;
    // parse array element until end of array
    while (!rb.eof()) {
        StringRef element(rb.position(), rb.count());
        bool has_quota = false;
        if (!next_element_from_string(rb, element, has_quota)) {
            // we should do array element column revert if error
            nested_column.pop_back(element_num);
            return Status::InvalidArgument("Cannot read array element from text '{}'",
                                           element.to_string());
        }

        // handle empty element
        if (element.size == 0) {
            auto& nested_null_col = reinterpret_cast<ColumnNullable&>(nested_column);
            nested_null_col.get_nested_column().insert_default();
            nested_null_col.get_null_map_data().push_back(0);
            ++element_num;
            continue;
        }

        // handle null element, need to distinguish null and "null"
        if (!has_quota && element.size == 4 && strncmp(element.data, "null", 4) == 0) {
            // insert null
            auto& nested_null_col = reinterpret_cast<ColumnNullable&>(nested_column);
            nested_null_col.get_nested_column().insert_default();
            nested_null_col.get_null_map_data().push_back(1);
            ++element_num;
            continue;
        }

        // handle normal element
        ReadBuffer read_buffer(const_cast<char*>(element.data), element.size);
        auto st = nested->from_string(read_buffer, &nested_column);
        if (!st.ok()) {
            // we should do array element column revert if error
            nested_column.pop_back(element_num);
            return st;
        }
        ++element_num;
    }
    offsets.push_back(offsets.back() + element_num);
    return Status::OK();
}

FieldWithDataType DataTypeArray::get_field_with_data_type(const IColumn& column,
                                                          size_t row_num) const {
    const auto& array_column = assert_cast<const ColumnArray&>(column);
    int precision = -1;
    int scale = -1;
    auto nested_type = get_nested_type();
    PrimitiveType nested_type_id = nested_type->get_primitive_type();
    uint8_t num_dimensions = 1;
    while (nested_type_id == TYPE_ARRAY) {
        nested_type = remove_nullable(nested_type);
        const auto& nested_array = assert_cast<const DataTypeArray&>(*nested_type);
        nested_type_id = nested_array.get_nested_type()->get_primitive_type();
        num_dimensions++;
    }
    if (is_decimal(nested_type_id)) {
        precision = nested_type->get_precision();
        scale = nested_type->get_scale();
    } else if (nested_type_id == TYPE_DATETIMEV2) {
        scale = nested_type->get_scale();
    } else if (nested_type_id == TYPE_JSONB) {
        // Array<Jsonb> should return JsonbField as element
        // Currently only Array<Jsonb> is supported
        DCHECK(num_dimensions == 1);
        Array arr;
        size_t offset = array_column.offset_at(row_num);
        size_t size = array_column.size_at(row_num);
        for (size_t i = 0; i < size; ++i) {
            auto field = Field::create_field<TYPE_JSONB>({});
            array_column.get_data().get(offset + i, field);
            arr.push_back(field);
        }
        return FieldWithDataType {
                .field = Field::create_field<TYPE_ARRAY>(arr),
                .base_scalar_type_id = nested_type_id,
                .num_dimensions = num_dimensions,
                .precision = precision,
                .scale = scale,
        };
    }
    auto field = array_column[row_num];
    return FieldWithDataType {
            .field = std::move(field),
            .base_scalar_type_id = nested_type_id,
            .num_dimensions = num_dimensions,
            .precision = precision,
            .scale = scale,
    };
}

} // namespace doris::vectorized
