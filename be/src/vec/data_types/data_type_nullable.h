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
// https://github.com/ClickHouse/ClickHouse/blob/master/src/DataTypes/DataTypeNullable.h
// and modified by Doris

#pragma once

#include <gen_cpp/Types_types.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "common/status.h"
#include "runtime/define_primitive_type.h"
#include "runtime/types.h"
#include "vec/core/field.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type.h"
#include "vec/data_types/serde/data_type_nullable_serde.h"
#include "vec/data_types/serde/data_type_serde.h"

namespace doris {
class PColumnMeta;

namespace vectorized {
class BufferWritable;
class IColumn;
class ReadBuffer;
} // namespace vectorized
} // namespace doris

namespace doris::vectorized {

/// A nullable data type is an ordinary data type provided with a tag
/// indicating that it also contains the NULL value. The following class
/// embodies this concept.
class DataTypeNullable final : public IDataType {
public:
    static constexpr PrimitiveType PType = TYPE_NULL;
    explicit DataTypeNullable(const DataTypePtr& nested_data_type_);
    std::string do_get_name() const override {
        return "Nullable(" + nested_data_type->get_name() + ")";
    }
    const std::string get_family_name() const override { return "Nullable"; }
    PrimitiveType get_primitive_type() const override {
        return nested_data_type->get_primitive_type();
    }

    doris::FieldType get_storage_field_type() const override {
        return nested_data_type->get_storage_field_type();
    }

    int64_t get_uncompressed_serialized_bytes(const IColumn& column,
                                              int be_exec_version) const override;
    char* serialize(const IColumn& column, char* buf, int be_exec_version) const override;
    const char* deserialize(const char* buf, MutableColumnPtr* column,
                            int be_exec_version) const override;
    void to_pb_column_meta(PColumnMeta* col_meta) const override;

    MutableColumnPtr create_column() const override;
    Status check_column(const IColumn& column) const override;

    Field get_default() const override;

    Field get_field(const TExprNode& node) const override {
        if (node.node_type == TExprNodeType::NULL_LITERAL) {
            return Field();
        }
        return nested_data_type->get_field(node);
    }

    bool equals(const IDataType& rhs) const override;

    bool is_value_unambiguously_represented_in_contiguous_memory_region() const override {
        return nested_data_type->is_value_unambiguously_represented_in_contiguous_memory_region();
    }

    bool have_subtypes() const override { return true; }
    bool should_align_right_in_pretty_formats() const override {
        return nested_data_type->should_align_right_in_pretty_formats();
    }
    bool text_can_contain_only_valid_utf8() const override {
        return nested_data_type->text_can_contain_only_valid_utf8();
    }
    bool is_comparable() const override { return nested_data_type->is_comparable(); }
    bool have_maximum_size_of_value() const override {
        return nested_data_type->have_maximum_size_of_value();
    }
    size_t get_size_of_value_in_memory() const override {
        return 1 + nested_data_type->get_size_of_value_in_memory();
    }
    bool is_nullable() const override { return true; }
    bool can_be_inside_low_cardinality() const override {
        return nested_data_type->can_be_inside_low_cardinality();
    }
    std::string to_string(const IColumn& column, size_t row_num) const override;
    void to_string(const IColumn& column, size_t row_num, BufferWritable& ostr) const override;
    Status from_string(ReadBuffer& rb, IColumn* column) const override;

    const DataTypePtr& get_nested_type() const { return nested_data_type; }
    bool is_null_literal() const override { return nested_data_type->is_null_literal(); }

    using SerDeType = DataTypeNullableSerDe;
    DataTypeSerDeSPtr get_serde(int nesting_level = 1) const override {
        return std::make_shared<SerDeType>(nested_data_type->get_serde(nesting_level),
                                           nesting_level);
    }
    UInt32 get_precision() const override { return nested_data_type->get_precision(); }
    UInt32 get_scale() const override { return nested_data_type->get_scale(); }
    void to_protobuf(PTypeDesc* ptype, PTypeNode* node, PScalarType* scalar_type) const override {
        nested_data_type->to_protobuf(ptype, node, scalar_type);
    }
    FieldWithDataType get_field_with_data_type(const IColumn& column,
                                               size_t row_num) const override;
#ifdef BE_TEST
    void to_thrift(TTypeDesc& thrift_type, TTypeNode& node) const override {
        nested_data_type->to_thrift(thrift_type, node);
    }
#endif

private:
    DataTypePtr nested_data_type;
};

DataTypePtr make_nullable(const DataTypePtr& type);
DataTypes make_nullable(const DataTypes& types);
DataTypePtr remove_nullable(const DataTypePtr& type);
DataTypes remove_nullable(const DataTypes& types);
bool have_nullable(const DataTypes& types);

} // namespace doris::vectorized
