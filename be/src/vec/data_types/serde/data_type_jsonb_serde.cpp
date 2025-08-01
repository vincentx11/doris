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

#include "data_type_jsonb_serde.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "arrow/array/builder_binary.h"
#include "common/exception.h"
#include "common/status.h"
#include "exprs/json_functions.h"
#include "runtime/jsonb_value.h"
#include "util/jsonb_parser_simd.h"
namespace doris {
namespace vectorized {
#include "common/compile_check_begin.h"

template <bool is_binary_format>
Status DataTypeJsonbSerDe::_write_column_to_mysql(const IColumn& column,
                                                  MysqlRowBuffer<is_binary_format>& result,
                                                  int64_t row_idx, bool col_const,
                                                  const FormatOptions& options) const {
    auto& data = assert_cast<const ColumnString&>(column);
    const auto col_index = index_check_const(row_idx, col_const);
    const auto jsonb_val = data.get_data_at(col_index);
    // jsonb size == 0 is NULL
    if (jsonb_val.data == nullptr || jsonb_val.size == 0) {
        if (UNLIKELY(0 != result.push_null())) {
            return Status::InternalError("pack mysql buffer failed.");
        }
    } else {
        std::string json_str = JsonbToJson::jsonb_to_json_string(jsonb_val.data, jsonb_val.size);
        if (UNLIKELY(0 != result.push_string(json_str.c_str(), json_str.size()))) {
            return Status::InternalError("pack mysql buffer failed.");
        }
    }
    return Status::OK();
}

Status DataTypeJsonbSerDe::write_column_to_mysql(const IColumn& column,
                                                 MysqlRowBuffer<true>& row_buffer, int64_t row_idx,
                                                 bool col_const,
                                                 const FormatOptions& options) const {
    return _write_column_to_mysql(column, row_buffer, row_idx, col_const, options);
}

Status DataTypeJsonbSerDe::write_column_to_mysql(const IColumn& column,
                                                 MysqlRowBuffer<false>& row_buffer, int64_t row_idx,
                                                 bool col_const,
                                                 const FormatOptions& options) const {
    return _write_column_to_mysql(column, row_buffer, row_idx, col_const, options);
}

Status DataTypeJsonbSerDe::serialize_column_to_json(const IColumn& column, int64_t start_idx,
                                                    int64_t end_idx, BufferWritable& bw,
                                                    FormatOptions& options) const {
    SERIALIZE_COLUMN_TO_JSON();
}

Status DataTypeJsonbSerDe::serialize_one_cell_to_json(const IColumn& column, int64_t row_num,
                                                      BufferWritable& bw,
                                                      FormatOptions& options) const {
    auto result = check_column_const_set_readability(column, row_num);
    ColumnPtr ptr = result.first;
    row_num = result.second;

    const StringRef& s = assert_cast<const ColumnString&>(*ptr).get_data_at(row_num);
    if (s.size > 0) {
        std::string str = JsonbToJson::jsonb_to_json_string(s.data, s.size);
        bw.write(str.c_str(), str.size());
    } else {
        bw.write(NULL_IN_CSV_FOR_ORDINARY_TYPE.c_str(),
                 strlen(NULL_IN_CSV_FOR_ORDINARY_TYPE.c_str()));
    }
    return Status::OK();
}

Status DataTypeJsonbSerDe::deserialize_column_from_json_vector(IColumn& column,
                                                               std::vector<Slice>& slices,
                                                               uint64_t* num_deserialized,
                                                               const FormatOptions& options) const {
    DESERIALIZE_COLUMN_FROM_JSON_VECTOR();
    return Status::OK();
}

Status DataTypeJsonbSerDe::deserialize_one_cell_from_json(IColumn& column, Slice& slice,
                                                          const FormatOptions& options) const {
    JsonBinaryValue value;
    RETURN_IF_ERROR(value.from_json_string(slice.data, slice.size));

    auto& column_string = assert_cast<ColumnString&>(column);
    column_string.insert_data(value.value(), value.size());
    return Status::OK();
}

Status DataTypeJsonbSerDe::write_column_to_arrow(const IColumn& column, const NullMap* null_map,
                                                 arrow::ArrayBuilder* array_builder, int64_t start,
                                                 int64_t end, const cctz::time_zone& ctz) const {
    const auto& string_column = assert_cast<const ColumnString&>(column);
    auto& builder = assert_cast<arrow::StringBuilder&>(*array_builder);
    for (size_t string_i = start; string_i < end; ++string_i) {
        if (null_map && (*null_map)[string_i]) {
            RETURN_IF_ERROR(checkArrowStatus(builder.AppendNull(), column.get_name(),
                                             array_builder->type()->name()));
            continue;
        }
        std::string_view string_ref = string_column.get_data_at(string_i).to_string_view();
        std::string json_string =
                JsonbToJson::jsonb_to_json_string(string_ref.data(), string_ref.size());
        RETURN_IF_ERROR(
                checkArrowStatus(builder.Append(json_string.data(),
                                                cast_set<int, size_t, false>(json_string.size())),
                                 column.get_name(), array_builder->type()->name()));
    }
    return Status::OK();
}

Status DataTypeJsonbSerDe::write_column_to_orc(const std::string& timezone, const IColumn& column,
                                               const NullMap* null_map,
                                               orc::ColumnVectorBatch* orc_col_batch, int64_t start,
                                               int64_t end, vectorized::Arena& arena) const {
    auto* cur_batch = dynamic_cast<orc::StringVectorBatch*>(orc_col_batch);
    const auto& string_column = assert_cast<const ColumnString&>(column);
    // First pass: calculate total memory needed and collect serialized values
    std::vector<std::string> serialized_values;
    std::vector<size_t> valid_row_indices;
    size_t total_size = 0;
    for (size_t row_id = start; row_id < end; row_id++) {
        if (cur_batch->notNull[row_id] == 1) {
            std::string_view string_ref = string_column.get_data_at(row_id).to_string_view();
            auto serialized_value =
                    JsonbToJson::jsonb_to_json_string(string_ref.data(), string_ref.size());
            serialized_values.push_back(std::move(serialized_value));
            size_t len = serialized_values.back().length();
            total_size += len;
            valid_row_indices.push_back(row_id);
        }
    }
    // Allocate continues memory based on calculated size
    char* ptr = arena.alloc(total_size);
    if (!ptr) {
        return Status::InternalError(
                "malloc memory {} error when write variant column data to orc file.", total_size);
    }
    // Second pass: copy data to allocated memory
    size_t offset = 0;
    for (size_t i = 0; i < serialized_values.size(); i++) {
        const auto& serialized_value = serialized_values[i];
        size_t row_id = valid_row_indices[i];
        size_t len = serialized_value.length();
        if (offset + len > total_size) {
            return Status::InternalError(
                    "Buffer overflow when writing column data to ORC file. offset {} with len {} "
                    "exceed total_size {} . ",
                    offset, len, total_size);
        }
        memcpy(ptr + offset, serialized_value.data(), len);
        cur_batch->data[row_id] = ptr + offset;
        cur_batch->length[row_id] = len;
        offset += len;
    }
    cur_batch->numElements = end - start;
    return Status::OK();
}

void convert_jsonb_to_rapidjson(const JsonbValue& val, rapidjson::Value& target,
                                rapidjson::Document::AllocatorType& allocator) {
    // convert type of jsonb to rapidjson::Value
    switch (val.type) {
    case JsonbType::T_True:
        target.SetBool(true);
        break;
    case JsonbType::T_False:
        target.SetBool(false);
        break;
    case JsonbType::T_Null:
        target.SetNull();
        break;
    case JsonbType::T_Float:
        target.SetFloat(val.unpack<JsonbFloatVal>()->val());
        break;
    case JsonbType::T_Double:
        target.SetDouble(val.unpack<JsonbDoubleVal>()->val());
        break;
    case JsonbType::T_Int64:
        target.SetInt64(val.unpack<JsonbInt64Val>()->val());
        break;
    case JsonbType::T_Int32:
        target.SetInt(val.unpack<JsonbInt32Val>()->val());
        break;
    case JsonbType::T_Int16:
        target.SetInt(val.unpack<JsonbInt16Val>()->val());
        break;
    case JsonbType::T_Int8:
        target.SetInt(val.unpack<JsonbInt8Val>()->val());
        break;
    case JsonbType::T_String:
        target.SetString(val.unpack<JsonbStringVal>()->getBlob(),
                         val.unpack<JsonbStringVal>()->getBlobLen());
        break;
    case JsonbType::T_Array: {
        target.SetArray();
        const ArrayVal& array = *val.unpack<ArrayVal>();
        if (array.numElem() == 0) {
            target.SetNull();
            break;
        }
        target.Reserve(array.numElem(), allocator);
        for (auto it = array.begin(); it != array.end(); ++it) {
            rapidjson::Value array_val;
            convert_jsonb_to_rapidjson(*static_cast<const JsonbValue*>(it), array_val, allocator);
            target.PushBack(array_val, allocator);
        }
        break;
    }
    case JsonbType::T_Object: {
        target.SetObject();
        const ObjectVal& obj = *val.unpack<ObjectVal>();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            rapidjson::Value obj_val;
            convert_jsonb_to_rapidjson(*it->value(), obj_val, allocator);
            target.AddMember(rapidjson::GenericStringRef(it->getKeyStr(), it->klen()), obj_val,
                             allocator);
        }
        break;
    }
    default:
        CHECK(false) << "unkown type " << static_cast<int>(val.type);
        break;
    }
}

Status DataTypeJsonbSerDe::write_one_cell_to_json(const IColumn& column, rapidjson::Value& result,
                                                  rapidjson::Document::AllocatorType& allocator,
                                                  Arena& mem_pool, int64_t row_num) const {
    const auto& data = assert_cast<const ColumnString&>(column);
    const auto jsonb_val = data.get_data_at(row_num);
    if (jsonb_val.empty()) {
        return Status::OK();
    }
    JsonbValue* val = JsonbDocument::createValue(jsonb_val.data, jsonb_val.size);
    if (val == nullptr) {
        return Status::InternalError("Failed to get json document from jsonb");
    }
    rapidjson::Value value;
    convert_jsonb_to_rapidjson(*val, value, allocator);
    if (val->isObject() && result.IsObject()) {
        JsonFunctions::merge_objects(result, value, allocator);
    } else {
        result = std::move(value);
    }
    return Status::OK();
}

Status DataTypeJsonbSerDe::read_one_cell_from_json(IColumn& column,
                                                   const rapidjson::Value& result) const {
    // TODO improve performance
    auto& col = assert_cast<ColumnString&>(column);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    result.Accept(writer);
    JsonBinaryValue jsonb_value;
    RETURN_IF_ERROR(jsonb_value.from_json_string(buffer.GetString(), buffer.GetLength()));
    col.insert_data(jsonb_value.value(), jsonb_value.size());
    return Status::OK();
}
Status DataTypeJsonbSerDe::write_column_to_pb(const IColumn& column, PValues& result, int64_t start,
                                              int64_t end) const {
    const auto& string_column = assert_cast<const ColumnString&>(column);
    result.mutable_string_value()->Reserve(cast_set<int>(end - start));
    auto* ptype = result.mutable_type();
    ptype->set_id(PGenericType::JSONB);
    for (size_t row_num = start; row_num < end; ++row_num) {
        const auto& string_ref = string_column.get_data_at(row_num);
        if (string_ref.size > 0) {
            result.add_string_value(
                    JsonbToJson::jsonb_to_json_string(string_ref.data, string_ref.size));
        } else {
            result.add_string_value(NULL_IN_CSV_FOR_ORDINARY_TYPE);
        }
    }
    return Status::OK();
}

Status DataTypeJsonbSerDe::read_column_from_pb(IColumn& column, const PValues& arg) const {
    auto& column_string = assert_cast<ColumnString&>(column);
    column_string.reserve(column_string.size() + arg.string_value_size());
    JsonBinaryValue value;
    for (int i = 0; i < arg.string_value_size(); ++i) {
        RETURN_IF_ERROR(value.from_json_string(arg.string_value(i)));
        column_string.insert_data(value.value(), value.size());
    }
    return Status::OK();
}
} // namespace vectorized
} // namespace doris
