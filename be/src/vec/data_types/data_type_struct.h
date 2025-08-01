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
// https://github.com/ClickHouse/ClickHouse/blob/master/src/DataTypes/DataTypeTuple.h
// and modified by Doris

#pragma once

#include <gen_cpp/Types_types.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/status.h"
#include "runtime/define_primitive_type.h"
#include "vec/core/field.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type.h"
#include "vec/data_types/serde/data_type_serde.h"
#include "vec/data_types/serde/data_type_struct_serde.h"

namespace doris {
class PColumnMeta;

namespace vectorized {
class BufferWritable;
class IColumn;
class ReadBuffer;
} // namespace vectorized
} // namespace doris

namespace doris::vectorized {

/** Struct data type.
  * Used as an intermediate result when evaluating expressions.
  * Also can be used as a column - the result of the query execution.
  *
  * Struct elements can have names.
  * If an element is unnamed, it will have automatically assigned name like '1', '2', '3' corresponding to its position.
  * Manually assigned names must not begin with digit. Names must be unique.
  *
  * All structs with same size and types of elements are equivalent for expressions, regardless to names of elements.
  */
class DataTypeStruct final : public IDataType {
private:
    DataTypes elems;
    Strings names;
    bool have_explicit_names;

public:
    static constexpr bool is_parametric = true;
    static constexpr PrimitiveType PType = TYPE_STRUCT;

    explicit DataTypeStruct(const DataTypes& elems);
    DataTypeStruct(const DataTypes& elems, const Strings& names);
    PrimitiveType get_primitive_type() const override { return PrimitiveType::TYPE_STRUCT; }

    doris::FieldType get_storage_field_type() const override {
        return doris::FieldType::OLAP_FIELD_TYPE_STRUCT;
    }
    std::string do_get_name() const override;
    const std::string get_family_name() const override { return "Struct"; }

    bool supports_sparse_serialization() const { return true; }

    MutableColumnPtr create_column() const override;
    Status check_column(const IColumn& column) const override;

    Field get_default() const override;

    Field get_field(const TExprNode& node) const override {
        throw doris::Exception(ErrorCode::NOT_IMPLEMENTED_ERROR,
                               "Unimplemented get_field for struct");
    }

    bool equals(const IDataType& rhs) const override;

    bool have_subtypes() const override { return !elems.empty(); }
    bool is_comparable() const override;
    bool text_can_contain_only_valid_utf8() const override;
    bool have_maximum_size_of_value() const override;
    size_t get_size_of_value_in_memory() const override;

    const DataTypePtr& get_element(size_t i) const { return elems[i]; }
    const DataTypes& get_elements() const { return elems; }
    const String& get_element_name(size_t i) const { return names[i]; }
    const Strings& get_element_names() const { return names; }

    size_t get_position_by_name(const String& name) const;
    std::optional<size_t> try_get_position_by_name(const String& name) const;
    String get_name_by_position(size_t i) const;

    int64_t get_uncompressed_serialized_bytes(const IColumn& column,
                                              int be_exec_version) const override;
    char* serialize(const IColumn& column, char* buf, int be_exec_version) const override;
    const char* deserialize(const char* buf, MutableColumnPtr* column,
                            int be_exec_version) const override;
    void to_pb_column_meta(PColumnMeta* col_meta) const override;

    Status from_string(ReadBuffer& rb, IColumn* column) const override;
    std::string to_string(const IColumn& column, size_t row_num) const override;
    void to_string(const IColumn& column, size_t row_num, BufferWritable& ostr) const override;
    bool get_have_explicit_names() const { return have_explicit_names; }
    using SerDeType = DataTypeStructSerDe;
    DataTypeSerDeSPtr get_serde(int nesting_level = 1) const override {
        DataTypeSerDeSPtrs ptrs;
        for (auto iter = elems.begin(); iter < elems.end(); ++iter) {
            ptrs.push_back((*iter)->get_serde(nesting_level + 1));
        }
        return std::make_shared<SerDeType>(ptrs, names, nesting_level);
    };
    void to_protobuf(PTypeDesc* ptype, PTypeNode* node, PScalarType* scalar_type) const override {
        node->set_type(TTypeNodeType::STRUCT);
        for (size_t i = 0; i < elems.size(); ++i) {
            auto field = node->add_struct_fields();
            field->set_name(get_element_name(i));
            field->set_contains_null(elems[i]->is_nullable());
        }
        for (const auto& child : elems) {
            child->to_protobuf(ptype);
        }
    }
#ifdef BE_TEST
    void to_thrift(TTypeDesc& thrift_type, TTypeNode& node) const override {
        node.type = TTypeNodeType::STRUCT;
        node.__set_struct_fields(std::vector<TStructField>());
        for (size_t i = 0; i < get_elements().size(); i++) {
            node.struct_fields.push_back(TStructField());
            node.struct_fields.back().name = get_element_name(i);
            node.struct_fields.back().contains_null = get_element(i)->is_nullable();
        }
        for (size_t i = 0; i < get_elements().size(); i++) {
            get_element(i)->to_thrift(thrift_type);
        }
    }
#endif
};

} // namespace doris::vectorized
