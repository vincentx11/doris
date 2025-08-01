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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <memory>
#include <utility>

#include "common/status.h"
#include "runtime/primitive_type.h"
#include "vec/aggregate_functions/aggregate_function.h"
#include "vec/columns/column.h"
#include "vec/columns/column_vector.h"
#include "vec/common/assert_cast.h"
#include "vec/core/block.h"
#include "vec/core/column_numbers.h"
#include "vec/core/column_with_type_and_name.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type.h"
#include "vec/data_types/data_type_nullable.h"
#include "vec/data_types/data_type_number.h"
#include "vec/functions/function.h"
#include "vec/functions/simple_function_factory.h"

namespace doris {
class FunctionContext;
} // namespace doris

namespace doris::vectorized {
class FunctionWidthBucket : public IFunction {
public:
    static constexpr auto name = "width_bucket";
    static FunctionPtr create() { return std::make_shared<FunctionWidthBucket>(); }

    /// Get function name.
    String get_name() const override { return name; }

    bool is_variadic() const override { return false; }

    size_t get_number_of_arguments() const override { return 4; }

    DataTypePtr get_return_type_impl(const DataTypes& arguments) const override {
        return std::make_shared<DataTypeInt64>();
    }

    Status execute_impl(FunctionContext* context, Block& block, const ColumnNumbers& arguments,
                        uint32_t result, size_t input_rows_count) const override {
        ColumnPtr expr_ptr =
                block.get_by_position(arguments[0]).column->convert_to_full_column_if_const();
        ColumnPtr min_value_ptr =
                block.get_by_position(arguments[1]).column->convert_to_full_column_if_const();
        ColumnPtr max_value_ptr =
                block.get_by_position(arguments[2]).column->convert_to_full_column_if_const();
        ColumnPtr num_buckets_ptr =
                block.get_by_position(arguments[3]).column->convert_to_full_column_if_const();
        int64_t num_buckets = num_buckets_ptr->get_int(0);

        if (num_buckets <= 0) {
            return Status::InternalError(
                    "The desired number({}) of buckets must be a positive integer value.",
                    num_buckets);
        }

        auto nested_column_ptr = ColumnInt64::create(input_rows_count, 0);
        DataTypePtr expr_type = block.get_by_position(arguments[0]).type;

        if (!_execute_by_type(*expr_ptr, *min_value_ptr, *max_value_ptr, num_buckets,
                              *nested_column_ptr, expr_type)) {
            return Status::InvalidArgument("Unsupported type for width_bucket: {}",
                                           expr_type->get_name());
        }

        block.replace_by_position(result, std::move(nested_column_ptr));
        return Status::OK();
    }

private:
    template <typename ColumnType>
    void _execute(const IColumn& expr_column, const IColumn& min_value_column,
                  const IColumn& max_value_column, const int64_t num_buckets,
                  IColumn& nested_column) const {
        const auto& expr_column_concrete = assert_cast<const ColumnType&>(expr_column);
        const auto& min_value_column_concrete = assert_cast<const ColumnType&>(min_value_column);
        const auto& max_value_column_concrete = assert_cast<const ColumnType&>(max_value_column);
        auto& nested_column_concrete = assert_cast<ColumnInt64&>(nested_column);

        size_t input_rows_count = expr_column.size();

        for (size_t i = 0; i < input_rows_count; ++i) {
            auto min_value = min_value_column_concrete.get_data()[i];
            auto max_value = max_value_column_concrete.get_data()[i];
            auto average_value = (max_value - min_value) / (1.0 * num_buckets);
            if (expr_column_concrete.get_data()[i] < min_value) {
                continue;
            } else if (expr_column_concrete.get_data()[i] >= max_value) {
                nested_column_concrete.get_data()[i] = num_buckets + 1;
            } else {
                if ((max_value - min_value) / num_buckets == 0) {
                    continue;
                }
                nested_column_concrete.get_data()[i] =
                        (int64_t)(1 +
                                  (expr_column_concrete.get_data()[i] - min_value) / average_value);
            }
        }
    }

    bool _execute_by_type(const IColumn& expr_column, const IColumn& min_value_column,
                          const IColumn& max_value_column, const int64_t num_buckets,
                          IColumn& nested_column_column, DataTypePtr& expr_type) const {
        switch (expr_type->get_primitive_type()) {
        case PrimitiveType::TYPE_TINYINT:
            _execute<ColumnInt8>(expr_column, min_value_column, max_value_column, num_buckets,
                                 nested_column_column);
            break;
        case PrimitiveType::TYPE_SMALLINT:
            _execute<ColumnInt16>(expr_column, min_value_column, max_value_column, num_buckets,
                                  nested_column_column);
            break;
        case PrimitiveType::TYPE_INT:
            _execute<ColumnInt32>(expr_column, min_value_column, max_value_column, num_buckets,
                                  nested_column_column);
            break;
        case PrimitiveType::TYPE_BIGINT:
            _execute<ColumnInt64>(expr_column, min_value_column, max_value_column, num_buckets,
                                  nested_column_column);
            break;
        case PrimitiveType::TYPE_FLOAT:
            _execute<ColumnFloat32>(expr_column, min_value_column, max_value_column, num_buckets,
                                    nested_column_column);
            break;
        case PrimitiveType::TYPE_DOUBLE:
            _execute<ColumnFloat64>(expr_column, min_value_column, max_value_column, num_buckets,
                                    nested_column_column);
            break;
        default:
            return false;
            break;
        }
        return true;
    }
};

void register_function_width_bucket(SimpleFunctionFactory& factory) {
    factory.register_function<FunctionWidthBucket>();
}

} // namespace doris::vectorized