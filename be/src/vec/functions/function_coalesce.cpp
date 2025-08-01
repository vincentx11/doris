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

#include <glog/logging.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <memory>
#include <utility>
#include <vector>

#include "common/status.h"
#include "runtime/runtime_state.h"
#include "vec/aggregate_functions/aggregate_function.h"
#include "vec/columns/column.h"
#include "vec/columns/column_complex.h"
#include "vec/columns/column_decimal.h"
#include "vec/columns/column_nullable.h"
#include "vec/columns/column_vector.h"
#include "vec/common/assert_cast.h"
#include "vec/core/block.h"
#include "vec/core/column_numbers.h"
#include "vec/core/column_with_type_and_name.h"
#include "vec/core/columns_with_type_and_name.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type.h"
#include "vec/data_types/data_type_nullable.h"
#include "vec/data_types/data_type_number.h"
#include "vec/functions/function.h"
#include "vec/functions/simple_function_factory.h"
#include "vec/utils/template_helpers.hpp"

namespace doris {
class FunctionContext;
} // namespace doris

namespace doris::vectorized {
class FunctionCoalesce : public IFunction {
public:
    static constexpr auto name = "coalesce";

    mutable FunctionBasePtr func_is_not_null;

    static FunctionPtr create() { return std::make_shared<FunctionCoalesce>(); }

    String get_name() const override { return name; }

    bool use_default_implementation_for_nulls() const override { return false; }

    bool is_variadic() const override { return true; }

    size_t get_number_of_arguments() const override { return 0; }

    DataTypePtr get_return_type_impl(const DataTypes& arguments) const override {
        DataTypePtr res;
        for (const auto& arg : arguments) {
            if (!arg->is_nullable()) {
                res = arg;
                break;
            }
        }

        res = res ? res : arguments[0];

        const ColumnsWithTypeAndName is_not_null_col {{nullptr, make_nullable(res), ""}};
        func_is_not_null = SimpleFunctionFactory::instance().get_function(
                "is_not_null_pred", is_not_null_col, std::make_shared<DataTypeUInt8>());

        return res;
    }

    Status execute_impl(FunctionContext* context, Block& block, const ColumnNumbers& arguments,
                        uint32_t result, size_t input_rows_count) const override {
        DCHECK_GE(arguments.size(), 1);
        DataTypePtr result_type = block.get_by_position(result).type;
        ColumnNumbers filtered_args;
        filtered_args.reserve(arguments.size());

        for (size_t i = 0; i < arguments.size(); ++i) {
            const auto& arg_type = block.get_by_position(arguments[i]).type;
            filtered_args.push_back(arguments[i]);
            if (!arg_type->is_nullable()) {
                if (i == 0) {
                    block.get_by_position(result).column =
                            block.get_by_position(arguments[0])
                                    .column->clone_resized(input_rows_count);
                    return Status::OK();
                } else {
                    break;
                }
            }
        }

        size_t remaining_rows = input_rows_count;
        size_t argument_size = filtered_args.size();
        std::vector<uint32_t> record_idx(
                input_rows_count,
                0); //used to save column idx, record the result data of each row from which column
        std::vector<uint8_t> filled_flags(
                input_rows_count,
                0); //used to save filled flag, in order to check current row whether have filled data

        MutableColumnPtr result_column;
        if (!result_type->is_nullable()) {
            result_column = result_type->create_column();
        } else {
            result_column = remove_nullable(result_type)->create_column();
        }

        // because now the string types does not support random position writing,
        // so insert into result data have two methods, one is for string types, one is for others type remaining
        bool is_string_result = result_column->is_column_string();
        if (is_string_result) {
            result_column->reserve(input_rows_count);
        }

        auto return_type = std::make_shared<DataTypeUInt8>();
        auto null_map = ColumnUInt8::create(
                input_rows_count, 1); //if null_map_data==1, the current row should be null
        auto* __restrict null_map_data = null_map->get_data().data();
        std::vector<ColumnPtr> argument_columns(
                argument_size); //use to save nested_column if is nullable column

        for (size_t i = 0; i < argument_size; ++i) {
            block.get_by_position(filtered_args[i]).column =
                    block.get_by_position(filtered_args[i])
                            .column->convert_to_full_column_if_const();
            argument_columns[i] = block.get_by_position(filtered_args[i]).column;
            if (const auto* nullable =
                        check_and_get_column<const ColumnNullable>(*argument_columns[i])) {
                argument_columns[i] = nullable->get_nested_column_ptr();
            }
        }

        Block temporary_block {
                ColumnsWithTypeAndName {block.get_by_position(filtered_args[0]),
                                        {nullptr, std::make_shared<DataTypeUInt8>(), ""}}};

        for (size_t i = 0; i < argument_size && remaining_rows; ++i) {
            temporary_block.get_by_position(0).column =
                    block.get_by_position(filtered_args[i]).column;
            RETURN_IF_ERROR(
                    func_is_not_null->execute(context, temporary_block, {0}, 1, input_rows_count));

            auto res_column =
                    (*temporary_block.get_by_position(1).column->convert_to_full_column_if_const())
                            .mutate();
            auto& res_map = assert_cast<ColumnUInt8*, TypeCheckOnRelease::DISABLE>(res_column.get())
                                    ->get_data();
            auto* __restrict res = res_map.data();

            // Here it's SIMD thought the compiler automatically
            // true: res[j]==1 && null_map_data[j]==1, false: others
            // if true: remaining_rows--; record_idx[j]=column_idx; null_map_data[j]=0, so the current row could fill result
            for (size_t j = 0; j < input_rows_count; ++j) {
                remaining_rows -= (res[j] & null_map_data[j]);
                record_idx[j] += (res[j] & null_map_data[j]) * i;
                null_map_data[j] -= (res[j] & null_map_data[j]);
            }

            if (remaining_rows == 0) {
                //check whether all result data from the same column
                size_t is_same_column_count = 0;
                const auto data = record_idx[0];
                for (size_t row = 0; row < input_rows_count; ++row) {
                    is_same_column_count += (record_idx[row] == data);
                }

                if (is_same_column_count == input_rows_count) {
                    if (result_type->is_nullable()) {
                        block.get_by_position(result).column =
                                make_nullable(argument_columns[i], false);
                    } else {
                        block.get_by_position(result).column = argument_columns[i];
                    }
                    return Status::OK();
                }
            }

            if (!is_string_result) {
                //if not string type, could check one column firstly,
                //and then fill the not null value in result column,
                //this method may result in higher CPU cache
                RETURN_IF_ERROR(filled_result_column(result_type, result_column,
                                                     argument_columns[i], null_map_data,
                                                     filled_flags.data(), input_rows_count));
            }
        }

        if (is_string_result) {
            //if string type,  should according to the record results, fill in result one by one,
            for (size_t row = 0; row < input_rows_count; ++row) {
                if (null_map_data[row]) { //should be null
                    result_column->insert_default();
                } else {
                    result_column->insert_from(*argument_columns[record_idx[row]].get(), row);
                }
            }
        }

        if (result_type->is_nullable()) {
            block.replace_by_position(
                    result, ColumnNullable::create(std::move(result_column), std::move(null_map)));
        } else {
            block.replace_by_position(result, std::move(result_column));
        }

        return Status::OK();
    }

    template <typename ColumnType>
    Status insert_result_data(MutableColumnPtr& result_column, ColumnPtr& argument_column,
                              const UInt8* __restrict null_map_data, UInt8* __restrict filled_flag,
                              const size_t input_rows_count) const {
        if (result_column->size() == 0 && input_rows_count) {
            result_column->resize(input_rows_count);
            auto* __restrict result_raw_data =
                    assert_cast<ColumnType*>(result_column.get())->get_data().data();
            for (int i = 0; i < input_rows_count; i++) {
                result_raw_data[i] = {};
            }
        }
        auto* __restrict result_raw_data =
                assert_cast<ColumnType*>(result_column.get())->get_data().data();
        auto* __restrict column_raw_data =
                assert_cast<const ColumnType*>(argument_column.get())->get_data().data();

        // Here it's SIMD thought the compiler automatically also
        // true: null_map_data[row]==0 && filled_idx[row]==0
        // if true, could filled current row data into result column
        for (size_t row = 0; row < input_rows_count; ++row) {
            result_raw_data[row] +=
                    column_raw_data[row] *
                    typename ColumnType::value_type(!(null_map_data[row] | filled_flag[row]));
            filled_flag[row] += (!(null_map_data[row] | filled_flag[row]));
        }
        return Status::OK();
    }

    Status insert_result_data_bitmap(MutableColumnPtr& result_column, ColumnPtr& argument_column,
                                     const UInt8* __restrict null_map_data,
                                     UInt8* __restrict filled_flag,
                                     const size_t input_rows_count) const {
        if (result_column->size() == 0 && input_rows_count) {
            result_column->resize(input_rows_count);
        }

        auto* __restrict result_raw_data =
                reinterpret_cast<ColumnBitmap*>(result_column.get())->get_data().data();
        auto* __restrict column_raw_data =
                reinterpret_cast<const ColumnBitmap*>(argument_column.get())->get_data().data();

        // Here it's SIMD thought the compiler automatically also
        // true: null_map_data[row]==0 && filled_idx[row]==0
        // if true, could filled current row data into result column
        for (size_t row = 0; row < input_rows_count; ++row) {
            if (!(null_map_data[row] | filled_flag[row])) {
                result_raw_data[row] = column_raw_data[row];
            }
            filled_flag[row] += (!(null_map_data[row] | filled_flag[row]));
        }
        return Status::OK();
    }

    [[nodiscard]] Status filled_result_column(const DataTypePtr& data_type,
                                              MutableColumnPtr& result_column,
                                              ColumnPtr& argument_column,
                                              UInt8* __restrict null_map_data,
                                              UInt8* __restrict filled_flag,
                                              const size_t input_rows_count) const {
        switch (data_type->get_primitive_type()) {
        case PrimitiveType::TYPE_BOOLEAN:
            return insert_result_data<ColumnUInt8>(result_column, argument_column, null_map_data,
                                                   filled_flag, input_rows_count);
        case PrimitiveType::TYPE_TINYINT:
            return insert_result_data<ColumnInt8>(result_column, argument_column, null_map_data,
                                                  filled_flag, input_rows_count);
        case PrimitiveType::TYPE_SMALLINT:
            return insert_result_data<ColumnInt16>(result_column, argument_column, null_map_data,
                                                   filled_flag, input_rows_count);
        case PrimitiveType::TYPE_INT:
            return insert_result_data<ColumnInt32>(result_column, argument_column, null_map_data,
                                                   filled_flag, input_rows_count);
        case PrimitiveType::TYPE_BIGINT:
            return insert_result_data<ColumnInt64>(result_column, argument_column, null_map_data,
                                                   filled_flag, input_rows_count);
        case PrimitiveType::TYPE_LARGEINT:
            return insert_result_data<ColumnInt128>(result_column, argument_column, null_map_data,
                                                    filled_flag, input_rows_count);
        case PrimitiveType::TYPE_FLOAT:
            return insert_result_data<ColumnFloat32>(result_column, argument_column, null_map_data,
                                                     filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DOUBLE:
            return insert_result_data<ColumnFloat64>(result_column, argument_column, null_map_data,
                                                     filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DECIMAL32:
            return insert_result_data<ColumnDecimal32>(
                    result_column, argument_column, null_map_data, filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DECIMAL64:
            return insert_result_data<ColumnDecimal64>(
                    result_column, argument_column, null_map_data, filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DECIMAL256:
            return insert_result_data<ColumnDecimal256>(
                    result_column, argument_column, null_map_data, filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DECIMALV2:
            return insert_result_data<ColumnDecimal128V2>(
                    result_column, argument_column, null_map_data, filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DECIMAL128I:
            return insert_result_data<ColumnDecimal128V3>(
                    result_column, argument_column, null_map_data, filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DATETIME:
            return insert_result_data<ColumnDateTime>(result_column, argument_column, null_map_data,
                                                      filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DATE:
            return insert_result_data<ColumnDate>(result_column, argument_column, null_map_data,
                                                  filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DATEV2:
            return insert_result_data<ColumnDateV2>(result_column, argument_column, null_map_data,
                                                    filled_flag, input_rows_count);
        case PrimitiveType::TYPE_DATETIMEV2:
            return insert_result_data<ColumnDateTimeV2>(
                    result_column, argument_column, null_map_data, filled_flag, input_rows_count);
        case PrimitiveType::TYPE_BITMAP:
            return insert_result_data_bitmap(result_column, argument_column, null_map_data,
                                             filled_flag, input_rows_count);
        default:
            return Status::NotSupported("argument_type {} not supported", data_type->get_name());
        }
    }
};

void register_function_coalesce(SimpleFunctionFactory& factory) {
    factory.register_function<FunctionCoalesce>();
}

} // namespace doris::vectorized
