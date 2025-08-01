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

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "common/status.h"
#include "vec/aggregate_functions/aggregate_function.h"
#include "vec/columns/column.h"
#include "vec/columns/column_array.h"
#include "vec/columns/column_complex.h"
#include "vec/columns/column_const.h"
#include "vec/columns/column_decimal.h"
#include "vec/columns/column_map.h"
#include "vec/columns/column_nullable.h"
#include "vec/columns/column_struct.h"
#include "vec/columns/column_variant.h"
#include "vec/common/assert_cast.h"
#include "vec/core/block.h"
#include "vec/core/column_numbers.h"
#include "vec/core/column_with_type_and_name.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type.h"
#include "vec/data_types/data_type_nullable.h"
#include "vec/functions/function.h"

namespace doris::vectorized {
#include "common/compile_check_begin.h"

template <bool has_else>
struct FunctionCaseName;

template <>
struct FunctionCaseName<false> {
    static constexpr auto name = "case";
};

template <>
struct FunctionCaseName<true> {
    static constexpr auto name = "case_has_else";
};

struct CaseWhenColumnHolder {
    using OptionalPtr = std::optional<ColumnPtr>;

    std::vector<OptionalPtr> when_ptrs; // case, when, when...
    std::vector<OptionalPtr> then_ptrs; // else, then, then...
    size_t pair_count;
    size_t rows_count;

    CaseWhenColumnHolder(Block& block, const ColumnNumbers& arguments, size_t input_rows_count,
                         bool has_else, bool when_null, bool then_null)
            : rows_count(input_rows_count) {
        when_ptrs.emplace_back(std::nullopt);
        then_ptrs.emplace_back(
                has_else
                        ? OptionalPtr(block.get_by_position(arguments[arguments.size() - 1]).column)
                        : std::nullopt);

        int begin = 0;
        int end = cast_set<int>(arguments.size() - has_else);
        pair_count = (end - begin) / 2 + 1; // when/then at [1: pair_count)

        for (int i = begin; i < end; i += 2) {
            when_ptrs.emplace_back(block.get_by_position(arguments[i]).column);
            then_ptrs.emplace_back(block.get_by_position(arguments[i + 1]).column);
        }

        // if case_column/when_column is nullable. cast all case_column/when_column to nullable.
        if (when_null) {
            for (OptionalPtr& column_ptr : when_ptrs) {
                cast_to_nullable(column_ptr);
            }
        }

        // if else_column/then_column is nullable. cast all else_column/then_column to nullable.
        if (then_null) {
            for (OptionalPtr& column_ptr : then_ptrs) {
                cast_to_nullable(column_ptr);
            }
        }
    }

    void cast_to_nullable(OptionalPtr& column_ptr) {
        if (!column_ptr.has_value() || column_ptr.value()->is_nullable()) {
            return;
        }
        column_ptr.emplace(make_nullable(column_ptr.value()));
    }
};

template <bool has_else>
class FunctionCase : public IFunction {
public:
    static constexpr auto name = FunctionCaseName<has_else>::name;
    static FunctionPtr create() { return std::make_shared<FunctionCase>(); }
    String get_name() const override { return name; }
    size_t get_number_of_arguments() const override { return 0; }
    bool is_variadic() const override { return true; }

    DataTypePtr get_return_type_impl(const DataTypes& arguments) const override {
        int loop_start = 1;
        int loop_end = cast_set<int>(has_else ? arguments.size() - 1 : arguments.size());

        bool is_nullable = false;
        if (!has_else || arguments[loop_end].get()->is_nullable()) {
            is_nullable = true;
        }
        for (int i = loop_start; !is_nullable && i < loop_end; i += 2) {
            if (arguments[i].get()->is_nullable()) {
                is_nullable = true;
            }
        }

        if (is_nullable) {
            return make_nullable(arguments[loop_start]);
        } else {
            return arguments[loop_start];
        }
    }

    bool use_default_implementation_for_nulls() const override { return false; }

    template <typename IndexType, typename ColumnType, bool when_null, bool then_null>
    Status execute_impl(const DataTypePtr& data_type, Block& block, uint32_t result,
                        CaseWhenColumnHolder column_holder) const {
        size_t rows_count = column_holder.rows_count;

        // `then` data index corresponding to each row of results, 0 represents `else`.
        auto then_idx_uptr = std::unique_ptr<IndexType[]>(new IndexType[rows_count]);
        IndexType* __restrict then_idx_ptr = then_idx_uptr.get();
        memset(then_idx_ptr, 0, sizeof(IndexType) * rows_count);

        auto case_column_ptr = column_holder.when_ptrs[0].value_or(nullptr);

        for (IndexType i = 1; i < column_holder.pair_count; i++) {
            auto when_column_ptr = column_holder.when_ptrs[i].value();
            if constexpr (when_null) {
                const auto* column_nullable_ptr =
                        assert_cast<const ColumnNullable*>(when_column_ptr.get());
                const auto* __restrict cond_raw_data =
                        assert_cast<const ColumnUInt8*>(
                                column_nullable_ptr->get_nested_column_ptr().get())
                                ->get_data()
                                .data();
                const auto* __restrict cond_raw_nullmap =
                        assert_cast<const ColumnUInt8*>(
                                column_nullable_ptr->get_null_map_column_ptr().get())
                                ->get_data()
                                .data();

                // simd automatically
                for (int row_idx = 0; row_idx < rows_count; row_idx++) {
                    then_idx_ptr[row_idx] |= (!then_idx_ptr[row_idx] * cond_raw_data[row_idx] *
                                              !cond_raw_nullmap[row_idx]) *
                                             i;
                }
            } else {
                const auto* __restrict cond_raw_data =
                        assert_cast<const ColumnUInt8*>(when_column_ptr.get())->get_data().data();

                // simd automatically
                for (int row_idx = 0; row_idx < rows_count; row_idx++) {
                    then_idx_ptr[row_idx] |= (!then_idx_ptr[row_idx]) * cond_raw_data[row_idx] * i;
                }
            }
        }

        return execute_update_result<IndexType, ColumnType, then_null>(data_type, result, block,
                                                                       then_idx_ptr, column_holder);
    }

    template <typename IndexType, typename ColumnType, bool then_null>
    Status execute_update_result(const DataTypePtr& data_type, uint32_t result, Block& block,
                                 const IndexType* then_idx,
                                 CaseWhenColumnHolder& column_holder) const {
        auto result_column_ptr = data_type->create_column();

        if constexpr (std::is_same_v<ColumnType, ColumnString> ||
                      std::is_same_v<ColumnType, ColumnBitmap> ||
                      std::is_same_v<ColumnType, ColumnArray> ||
                      std::is_same_v<ColumnType, ColumnMap> ||
                      std::is_same_v<ColumnType, ColumnStruct> ||
                      std::is_same_v<ColumnType, ColumnVariant> ||
                      std::is_same_v<ColumnType, ColumnHLL> ||
                      std::is_same_v<ColumnType, ColumnQuantileState> ||
                      std::is_same_v<ColumnType, ColumnIPv4> ||
                      std::is_same_v<ColumnType, ColumnIPv6>) {
            // result_column and all then_column is not nullable.
            // can't simd when type is string.
            update_result_normal<IndexType, ColumnType, then_null>(result_column_ptr, then_idx,
                                                                   column_holder);
        } else if constexpr (then_null || !std::is_same_v<IndexType, uint8_t>) {
            // result_column and all then_column is nullable.
            // TODO: make here simd automatically.
            update_result_normal<IndexType, ColumnType, then_null>(result_column_ptr, then_idx,
                                                                   column_holder);
        } else {
            update_result_auto_simd<ColumnType>(result_column_ptr, then_idx, column_holder);
        }

        block.replace_by_position(result, std::move(result_column_ptr));
        return Status::OK();
    }

    template <typename IndexType, typename ColumnType, bool then_null>
    void update_result_normal(MutableColumnPtr& result_column_ptr,
                              const IndexType* __restrict then_idx,
                              CaseWhenColumnHolder& column_holder) const {
        std::vector<uint8_t> is_consts(column_holder.then_ptrs.size());
        std::vector<ColumnPtr> raw_columns(column_holder.then_ptrs.size());
        for (size_t i = 0; i < column_holder.then_ptrs.size(); i++) {
            if (column_holder.then_ptrs[i].has_value()) {
                std::tie(raw_columns[i], is_consts[i]) =
                        unpack_if_const(column_holder.then_ptrs[i].value());
            }
        }

        auto* raw_result_column = result_column_ptr.get();
        for (int row_idx = 0; row_idx < column_holder.rows_count; row_idx++) {
            if constexpr (!has_else) {
                if (!then_idx[row_idx]) {
                    assert_cast<ColumnNullable*, TypeCheckOnRelease::DISABLE>(raw_result_column)
                            ->insert_default();
                    continue;
                }
            }
            size_t target = is_consts[then_idx[row_idx]] ? 0 : row_idx;
            if constexpr (then_null) {
                assert_cast<ColumnNullable*, TypeCheckOnRelease::DISABLE>(result_column_ptr.get())
                        ->insert_from_with_type<ColumnType>(*raw_columns[then_idx[row_idx]],
                                                            target);
            } else {
                assert_cast<ColumnType*, TypeCheckOnRelease::DISABLE>(result_column_ptr.get())
                        ->insert_from(*raw_columns[then_idx[row_idx]], target);
            }
        }
    }

    template <typename ColumnType>
    void update_result_auto_simd(MutableColumnPtr& result_column_ptr,
                                 const uint8_t* __restrict then_idx,
                                 CaseWhenColumnHolder& column_holder) const {
        for (auto& then_ptr : column_holder.then_ptrs) {
            then_ptr->reset(then_ptr.value()->convert_to_full_column_if_const().get());
        }

        size_t rows_count = column_holder.rows_count;
        result_column_ptr->resize(rows_count);
        auto* __restrict result_raw_data =
                assert_cast<ColumnType*, TypeCheckOnRelease::DISABLE>(result_column_ptr.get())
                        ->get_data()
                        .data();

        // set default value
        for (int i = 0; i < rows_count; i++) {
            result_raw_data[i] = {};
        }

        // some types had simd automatically, but some not.
        for (uint8_t i = (has_else ? 0 : 1); i < column_holder.pair_count; i++) {
            auto* __restrict column_raw_data =
                    assert_cast<ColumnType*>(
                            column_holder.then_ptrs[i].value()->assume_mutable().get())
                            ->get_data()
                            .data();

            for (int row_idx = 0; row_idx < rows_count; row_idx++) {
                result_raw_data[row_idx] +=
                        typename ColumnType::value_type(then_idx[row_idx] == i) *
                        column_raw_data[row_idx];
            }
        }
    }

    template <typename ColumnType, bool when_null>
    Status execute_get_then_null(const DataTypePtr& data_type, Block& block,
                                 const ColumnNumbers& arguments, uint32_t result,
                                 size_t input_rows_count) const {
        bool then_null = false;
        for (int i = 1; i < arguments.size() - has_else; i += 2) {
            if (block.get_by_position(arguments[i]).type->is_nullable()) {
                then_null = true;
            }
        }
        if constexpr (has_else) {
            if (block.get_by_position(arguments[arguments.size() - 1]).type->is_nullable()) {
                then_null = true;
            }
        } else {
            then_null = true;
        }

        CaseWhenColumnHolder column_holder = CaseWhenColumnHolder(
                block, arguments, input_rows_count, has_else, when_null, then_null);
        if (column_holder.pair_count > UINT16_MAX) {
            return Status::NotSupported(
                    "case when do not support more than UINT16_MAX pairs conditions");
        }
        if (then_null) {
            if (column_holder.pair_count > UINT8_MAX) {
                return execute_impl<uint16_t, ColumnType, when_null, true>(data_type, block, result,
                                                                           column_holder);
            }
            return execute_impl<uint8_t, ColumnType, when_null, true>(data_type, block, result,
                                                                      column_holder);
        } else {
            if (column_holder.pair_count > UINT8_MAX) {
                return execute_impl<uint16_t, ColumnType, when_null, false>(data_type, block,
                                                                            result, column_holder);
            }
            return execute_impl<uint8_t, ColumnType, when_null, false>(data_type, block, result,
                                                                       column_holder);
        }
    }

    template <typename ColumnType>
    Status execute_get_when_null(const DataTypePtr& data_type, Block& block,
                                 const ColumnNumbers& arguments, uint32_t result,
                                 size_t input_rows_count) const {
        bool when_null = false;
        for (int i = 0; i < arguments.size() - has_else; i += 2) {
            block.replace_by_position_if_const(arguments[i]);
            if (block.get_by_position(arguments[i]).type->is_nullable()) {
                when_null = true;
            }
        }

        if (when_null) {
            return execute_get_then_null<ColumnType, true>(data_type, block, arguments, result,
                                                           input_rows_count);
        } else {
            return execute_get_then_null<ColumnType, false>(data_type, block, arguments, result,
                                                            input_rows_count);
        }
    }

    Status execute_get_type(const DataTypePtr& data_type, Block& block,
                            const ColumnNumbers& arguments, uint32_t result,
                            size_t input_rows_count) const {
        switch (data_type->get_primitive_type()) {
        case PrimitiveType::TYPE_BOOLEAN:
            return execute_get_when_null<ColumnUInt8>(data_type, block, arguments, result,
                                                      input_rows_count);
        case PrimitiveType::TYPE_TINYINT:
            return execute_get_when_null<ColumnInt8>(data_type, block, arguments, result,
                                                     input_rows_count);
        case PrimitiveType::TYPE_SMALLINT:
            return execute_get_when_null<ColumnInt16>(data_type, block, arguments, result,
                                                      input_rows_count);
        case PrimitiveType::TYPE_INT:
            return execute_get_when_null<ColumnInt32>(data_type, block, arguments, result,
                                                      input_rows_count);
        case PrimitiveType::TYPE_BIGINT:
            return execute_get_when_null<ColumnInt64>(data_type, block, arguments, result,
                                                      input_rows_count);
        case PrimitiveType::TYPE_LARGEINT:
            return execute_get_when_null<ColumnInt128>(data_type, block, arguments, result,
                                                       input_rows_count);
        case PrimitiveType::TYPE_FLOAT:
            return execute_get_when_null<ColumnFloat32>(data_type, block, arguments, result,
                                                        input_rows_count);
        case PrimitiveType::TYPE_DOUBLE:
            return execute_get_when_null<ColumnFloat64>(data_type, block, arguments, result,
                                                        input_rows_count);
        case PrimitiveType::TYPE_DECIMAL32:
            return execute_get_when_null<ColumnDecimal32>(data_type, block, arguments, result,
                                                          input_rows_count);
        case PrimitiveType::TYPE_DECIMAL64:
            return execute_get_when_null<ColumnDecimal64>(data_type, block, arguments, result,
                                                          input_rows_count);
        case PrimitiveType::TYPE_DECIMAL256:
            return execute_get_when_null<ColumnDecimal256>(data_type, block, arguments, result,
                                                           input_rows_count);
        case PrimitiveType::TYPE_DECIMAL128I:
            return execute_get_when_null<ColumnDecimal128V3>(data_type, block, arguments, result,
                                                             input_rows_count);
        case PrimitiveType::TYPE_DECIMALV2:
            return execute_get_when_null<ColumnDecimal128V2>(data_type, block, arguments, result,
                                                             input_rows_count);
        case PrimitiveType::TYPE_STRING:
        case PrimitiveType::TYPE_CHAR:
        case PrimitiveType::TYPE_VARCHAR:
        case PrimitiveType::TYPE_JSONB:
            return execute_get_when_null<ColumnString>(data_type, block, arguments, result,
                                                       input_rows_count);
        case PrimitiveType::TYPE_DATE:
            return execute_get_when_null<ColumnDate>(data_type, block, arguments, result,
                                                     input_rows_count);
        case PrimitiveType::TYPE_DATETIME:
            return execute_get_when_null<ColumnDateTime>(data_type, block, arguments, result,
                                                         input_rows_count);
        case PrimitiveType::TYPE_DATEV2:
            return execute_get_when_null<ColumnDateV2>(data_type, block, arguments, result,
                                                       input_rows_count);
        case PrimitiveType::TYPE_DATETIMEV2:
            return execute_get_when_null<ColumnDateTimeV2>(data_type, block, arguments, result,
                                                           input_rows_count);
        case PrimitiveType::TYPE_IPV6:
            return execute_get_when_null<ColumnIPv6>(data_type, block, arguments, result,
                                                     input_rows_count);
        case PrimitiveType::TYPE_IPV4:
            return execute_get_when_null<ColumnIPv4>(data_type, block, arguments, result,
                                                     input_rows_count);
        case PrimitiveType::TYPE_ARRAY:
            return execute_get_when_null<ColumnArray>(data_type, block, arguments, result,
                                                      input_rows_count);
        case PrimitiveType::TYPE_MAP:
            return execute_get_when_null<ColumnMap>(data_type, block, arguments, result,
                                                    input_rows_count);
        case PrimitiveType::TYPE_STRUCT:
            return execute_get_when_null<ColumnStruct>(data_type, block, arguments, result,
                                                       input_rows_count);
        case PrimitiveType::TYPE_VARIANT:
            return execute_get_when_null<ColumnVariant>(data_type, block, arguments, result,
                                                        input_rows_count);
        case PrimitiveType::TYPE_BITMAP:
            return execute_get_when_null<ColumnBitmap>(data_type, block, arguments, result,
                                                       input_rows_count);
        case PrimitiveType::TYPE_HLL:
            return execute_get_when_null<ColumnHLL>(data_type, block, arguments, result,
                                                    input_rows_count);
        case PrimitiveType::TYPE_QUANTILE_STATE:
            return execute_get_when_null<ColumnQuantileState>(data_type, block, arguments, result,
                                                              input_rows_count);
        default:
            return Status::NotSupported("argument_type {} not supported", data_type->get_name());
        }
    }

    Status execute_impl(FunctionContext* context, Block& block, const ColumnNumbers& arguments,
                        uint32_t result, size_t input_rows_count) const override {
        return execute_get_type(block.get_by_position(result).type, block, arguments, result,
                                input_rows_count);
    }
};

#include "common/compile_check_end.h"
} // namespace doris::vectorized
