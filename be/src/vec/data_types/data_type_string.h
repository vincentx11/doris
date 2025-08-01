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
// https://github.com/ClickHouse/ClickHouse/blob/master/src/DataTypes/DataTypeString.h
// and modified by Doris

#pragma once

#include <gen_cpp/Types_types.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "common/status.h"
#include "runtime/define_primitive_type.h"
#include "serde/data_type_string_serde.h"
#include "vec/columns/column_string.h"
#include "vec/core/field.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type.h"
#include "vec/data_types/serde/data_type_serde.h"

namespace doris::vectorized {
class BufferWritable;
class IColumn;
class ReadBuffer;

class DataTypeString : public IDataType {
public:
    using ColumnType = ColumnString;
    using FieldType = String;
    static constexpr PrimitiveType PType = TYPE_STRING;
    static constexpr bool is_parametric = false;

    const std::string get_family_name() const override { return "String"; }

    DataTypeString(int len = -1, PrimitiveType primitive_type = PrimitiveType::TYPE_STRING)
            : _len(len), _primitive_type(primitive_type) {
        DCHECK(is_string_type(primitive_type)) << primitive_type;
    }
    PrimitiveType get_primitive_type() const override { return _primitive_type; }

    doris::FieldType get_storage_field_type() const override {
        return doris::FieldType::OLAP_FIELD_TYPE_STRING;
    }

    int64_t get_uncompressed_serialized_bytes(const IColumn& column,
                                              int be_exec_version) const override;
    char* serialize(const IColumn& column, char* buf, int be_exec_version) const override;
    const char* deserialize(const char* buf, MutableColumnPtr* column,
                            int be_exec_version) const override;
    MutableColumnPtr create_column() const override;
    Status check_column(const IColumn& column) const override;

    Field get_default() const override;

    Field get_field(const TExprNode& node) const override {
        DCHECK_EQ(node.node_type, TExprNodeType::STRING_LITERAL);
        DCHECK(node.__isset.string_literal);
        return Field::create_field<TYPE_STRING>(node.string_literal.value);
    }

    FieldWithDataType get_field_with_data_type(const IColumn& column,
                                               size_t row_num) const override;

    bool equals(const IDataType& rhs) const override;

    bool have_subtypes() const override { return false; }
    bool is_comparable() const override { return true; }
    bool is_value_unambiguously_represented_in_contiguous_memory_region() const override {
        return true;
    }
    bool can_be_inside_low_cardinality() const override { return true; }
    std::string to_string(const IColumn& column, size_t row_num) const override;
    void to_string(const IColumn& column, size_t row_num, BufferWritable& ostr) const override;
    Status from_string(ReadBuffer& rb, IColumn* column) const override;
    using SerDeType = DataTypeStringSerDe;
    DataTypeSerDeSPtr get_serde(int nesting_level = 1) const override {
        return std::make_shared<SerDeType>(nesting_level);
    };
    bool is_char_type() const { return _primitive_type == PrimitiveType::TYPE_CHAR; }
    int len() const { return _len; }
    void to_protobuf(PTypeDesc* ptype, PTypeNode* node, PScalarType* scalar_type) const override {
        if (_primitive_type == TYPE_CHAR || _primitive_type == TYPE_VARCHAR) {
            scalar_type->set_len(_len);
        }
    }
#ifdef BE_TEST
    void to_thrift(TTypeDesc& thrift_type, TTypeNode& node) const override {
        IDataType::to_thrift(thrift_type, node);
        TScalarType& scalar_type = node.scalar_type;
        scalar_type.__set_len(_len);
    }
#endif

private:
    // Fixed-length `char` type.
    const int _len;
    const PrimitiveType _primitive_type;
};

template <typename T>
constexpr static bool IsStringType = false;
template <>
inline constexpr bool IsStringType<DataTypeString> = true;

} // namespace doris::vectorized
