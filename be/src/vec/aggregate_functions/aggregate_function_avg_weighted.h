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

#include <stddef.h>

#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <cmath>
#include <memory>
#include <type_traits>

#include "common/status.h"
#include "runtime/decimalv2_value.h"
#include "util/binary_cast.hpp"
#include "vec/aggregate_functions/aggregate_function.h"
#include "vec/columns/column_vector.h"
#include "vec/common/assert_cast.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type_number.h"
#include "vec/io/io_helper.h"

namespace doris::vectorized {
#include "common/compile_check_begin.h"
class Arena;
class BufferReadable;
class BufferWritable;
class IColumn;
template <PrimitiveType T>
class ColumnDecimal;

template <PrimitiveType T>
struct AggregateFunctionAvgWeightedData {
    using DataType = typename PrimitiveTypeTraits<T>::ColumnItemType;
    void add(const DataType& data_val, double weight_val) {
#ifdef __clang__
#pragma clang fp reassociate(on)
#endif
        data_sum = data_sum + (double(data_val) * weight_val);
        weight_sum = weight_sum + weight_val;
    }

    void write(BufferWritable& buf) const {
        buf.write_binary(data_sum);
        buf.write_binary(weight_sum);
    }

    void read(BufferReadable& buf) {
        buf.read_binary(data_sum);
        buf.read_binary(weight_sum);
    }

    void merge(const AggregateFunctionAvgWeightedData& rhs) {
#ifdef __clang__
#pragma clang fp reassociate(on)
#endif
        data_sum = data_sum + rhs.data_sum;
        weight_sum = weight_sum + rhs.weight_sum;
    }

    void reset() {
        data_sum = 0.0;
        weight_sum = 0.0;
    }

    double get() const { return data_sum / weight_sum; }

    double data_sum = 0.0;
    double weight_sum = 0.0;
};

template <PrimitiveType type>
class AggregateFunctionAvgWeight final
        : public IAggregateFunctionDataHelper<AggregateFunctionAvgWeightedData<type>,
                                              AggregateFunctionAvgWeight<type>> {
public:
    using T = typename PrimitiveTypeTraits<type>::CppType;
    using ColVecType = typename PrimitiveTypeTraits<type>::ColumnType;

    AggregateFunctionAvgWeight(const DataTypes& argument_types_)
            : IAggregateFunctionDataHelper<AggregateFunctionAvgWeightedData<type>,
                                           AggregateFunctionAvgWeight<type>>(argument_types_) {}

    String get_name() const override { return "avg_weighted"; }

    DataTypePtr get_return_type() const override { return std::make_shared<DataTypeFloat64>(); }

    void add(AggregateDataPtr __restrict place, const IColumn** columns, ssize_t row_num,
             Arena&) const override {
        const auto& column =
                assert_cast<const ColVecType&, TypeCheckOnRelease::DISABLE>(*columns[0]);
        const auto& weight =
                assert_cast<const ColumnFloat64&, TypeCheckOnRelease::DISABLE>(*columns[1]);
        this->data(place).add(column.get_data()[row_num], weight.get_element(row_num));
    }

    void reset(AggregateDataPtr place) const override { this->data(place).reset(); }

    void merge(AggregateDataPtr __restrict place, ConstAggregateDataPtr rhs,
               Arena&) const override {
        this->data(place).merge(this->data(rhs));
    }

    void serialize(ConstAggregateDataPtr __restrict place, BufferWritable& buf) const override {
        this->data(place).write(buf);
    }

    void deserialize(AggregateDataPtr __restrict place, BufferReadable& buf,
                     Arena&) const override {
        this->data(place).read(buf);
    }

    void insert_result_into(ConstAggregateDataPtr __restrict place, IColumn& to) const override {
        auto& column = assert_cast<ColumnFloat64&>(to);
        column.get_data().push_back(this->data(place).get());
    }
};

} // namespace doris::vectorized

#include "common/compile_check_end.h"
