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

#include "vec/aggregate_functions/aggregate_function.h"
#include "vec/aggregate_functions/aggregate_function_simple_factory.h"
#include "vec/aggregate_functions/aggregate_function_statistic.h"
#include "vec/aggregate_functions/helpers.h"
#include "vec/data_types/data_type.h"

namespace doris::vectorized {
#include "common/compile_check_begin.h"

template <PrimitiveType T>
AggregateFunctionPtr type_dispatch_for_aggregate_function_skew(const DataTypes& argument_types,
                                                               const bool result_is_nullable,
                                                               const AggregateFunctionAttr& attr,
                                                               bool nullable_input) {
    using StatFunctionTemplate = StatFuncOneArg<T, 3>;

    if (nullable_input) {
        return creator_without_type::create_ignore_nullable<
                AggregateFunctionVarianceSimple<StatFunctionTemplate, true>>(
                argument_types, result_is_nullable, attr, STATISTICS_FUNCTION_KIND::SKEW_POP);
    } else {
        return creator_without_type::create_ignore_nullable<
                AggregateFunctionVarianceSimple<StatFunctionTemplate, false>>(
                argument_types, result_is_nullable, attr, STATISTICS_FUNCTION_KIND::SKEW_POP);
    }
};

AggregateFunctionPtr create_aggregate_function_skew(const std::string& name,
                                                    const DataTypes& argument_types,
                                                    const bool result_is_nullable,
                                                    const AggregateFunctionAttr& attr) {
    if (argument_types.size() != 1) {
        LOG(WARNING) << "aggregate function " << name << " requires exactly 1 argument";
        return nullptr;
    }

    if (!result_is_nullable) {
        LOG(WARNING) << "aggregate function " << name << " requires nullable result type";
        return nullptr;
    }

    const bool nullable_input = argument_types[0]->is_nullable();
    switch (argument_types[0]->get_primitive_type()) {
    case PrimitiveType::TYPE_BOOLEAN:
        return type_dispatch_for_aggregate_function_skew<TYPE_BOOLEAN>(
                argument_types, result_is_nullable, attr, nullable_input);
    case PrimitiveType::TYPE_TINYINT:
        return type_dispatch_for_aggregate_function_skew<TYPE_TINYINT>(
                argument_types, result_is_nullable, attr, nullable_input);
    case PrimitiveType::TYPE_SMALLINT:
        return type_dispatch_for_aggregate_function_skew<TYPE_SMALLINT>(
                argument_types, result_is_nullable, attr, nullable_input);
    case PrimitiveType::TYPE_INT:
        return type_dispatch_for_aggregate_function_skew<TYPE_INT>(
                argument_types, result_is_nullable, attr, nullable_input);
    case PrimitiveType::TYPE_BIGINT:
        return type_dispatch_for_aggregate_function_skew<TYPE_BIGINT>(
                argument_types, result_is_nullable, attr, nullable_input);
    case PrimitiveType::TYPE_LARGEINT:
        return type_dispatch_for_aggregate_function_skew<TYPE_LARGEINT>(
                argument_types, result_is_nullable, attr, nullable_input);
    case PrimitiveType::TYPE_FLOAT:
        return type_dispatch_for_aggregate_function_skew<TYPE_FLOAT>(
                argument_types, result_is_nullable, attr, nullable_input);
    case PrimitiveType::TYPE_DOUBLE:
        return type_dispatch_for_aggregate_function_skew<TYPE_DOUBLE>(
                argument_types, result_is_nullable, attr, nullable_input);
    default:
        LOG(WARNING) << "unsupported input type " << argument_types[0]->get_name()
                     << " for aggregate function " << name;
        return nullptr;
    }
}

void register_aggregate_function_skewness(AggregateFunctionSimpleFactory& factory) {
    factory.register_function_both("skew", create_aggregate_function_skew);
    factory.register_alias("skew", "skew_pop");
    factory.register_alias("skew", "skewness");
}

} // namespace doris::vectorized