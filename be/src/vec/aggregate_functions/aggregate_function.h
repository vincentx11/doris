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
// https://github.com/ClickHouse/ClickHouse/blob/master/src/AggregateFunctions/IAggregateFunction.h
// and modified by Doris

#pragma once

#include <utility>

#include "common/exception.h"
#include "common/status.h"
#include "util/defer_op.h"
#include "vec/columns/column_complex.h"
#include "vec/columns/column_string.h"
#include "vec/common/assert_cast.h"
#include "vec/common/hash_table/phmap_fwd_decl.h"
#include "vec/common/string_buffer.hpp"
#include "vec/core/block.h"
#include "vec/core/column_numbers.h"
#include "vec/core/field.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type_nullable.h"
#include "vec/data_types/data_type_string.h"

namespace doris::vectorized {
#include "common/compile_check_begin.h"

class Arena;
class IColumn;
class IDataType;

struct AggregateFunctionAttr {
    bool enable_decimal256 {false};
    bool is_window_function {false};
    std::vector<std::string> column_names;
};

template <bool nullable, typename ColVecType>
class AggregateFunctionBitmapCount;
template <typename Op>
class AggregateFunctionBitmapOp;
struct AggregateFunctionBitmapUnionOp;
class IAggregateFunction;
using AggregateFunctionPtr = std::shared_ptr<IAggregateFunction>;

using DataTypePtr = std::shared_ptr<const IDataType>;
using DataTypes = std::vector<DataTypePtr>;

using AggregateDataPtr = char*;
using ConstAggregateDataPtr = const char*;

#define SAFE_CREATE(create, destroy) \
    do {                             \
        try {                        \
            create;                  \
        } catch (...) {              \
            destroy;                 \
            throw;                   \
        }                            \
    } while (0)

/** Aggregate functions interface.
  * Instances of classes with this interface do not contain the data itself for aggregation,
  *  but contain only metadata (description) of the aggregate function,
  *  as well as methods for creating, deleting and working with data.
  * The data resulting from the aggregation (intermediate computing states) is stored in other objects
  *  (which can be created in some memory pool),
  *  and IAggregateFunction is the external interface for manipulating them.
  */
class IAggregateFunction {
public:
    IAggregateFunction(DataTypes argument_types_) : argument_types(std::move(argument_types_)) {}

    /// Get main function name.
    virtual String get_name() const = 0;

    /// Get the result type.
    virtual DataTypePtr get_return_type() const = 0;

    virtual ~IAggregateFunction() = default;

    /** Create empty data for aggregation with `placement new` at the specified location.
      * You will have to destroy them using the `destroy` method.
      */
    virtual void create(AggregateDataPtr __restrict place) const = 0;

    /// Delete data for aggregation.
    virtual void destroy(AggregateDataPtr __restrict place) const noexcept = 0;

    virtual void destroy_vec(AggregateDataPtr __restrict place,
                             const size_t num_rows) const noexcept = 0;

    /// Reset aggregation state
    virtual void reset(AggregateDataPtr place) const = 0;

    /// It is not necessary to delete data.
    virtual bool has_trivial_destructor() const = 0;

    /// Get `sizeof` of structure with data.
    virtual size_t size_of_data() const = 0;

    /// How the data structure should be aligned. NOTE: Currently not used (structures with aggregation state are put without alignment).
    virtual size_t align_of_data() const = 0;

    /** Adds a value into aggregation data on which place points to.
     *  columns points to columns containing arguments of aggregation function.
     *  row_num is number of row which should be added.
     *  Additional parameter arena should be used instead of standard memory allocator if the addition requires memory allocation.
     */
    virtual void add(AggregateDataPtr __restrict place, const IColumn** columns, ssize_t row_num,
                     Arena&) const = 0;

    virtual void add_many(AggregateDataPtr __restrict place, const IColumn** columns,
                          std::vector<int>& rows, Arena&) const {}

    /// Merges state (on which place points to) with other state of current aggregation function.
    virtual void merge(AggregateDataPtr __restrict place, ConstAggregateDataPtr rhs,
                       Arena&) const = 0;

    virtual void merge_vec(const AggregateDataPtr* places, size_t offset, ConstAggregateDataPtr rhs,
                           Arena&, const size_t num_rows) const = 0;

    // same as merge_vec, but only call "merge" function when place is not nullptr
    virtual void merge_vec_selected(const AggregateDataPtr* places, size_t offset,
                                    ConstAggregateDataPtr rhs, Arena&,
                                    const size_t num_rows) const = 0;

    /// Serializes state (to transmit it over the network, for example).
    virtual void serialize(ConstAggregateDataPtr __restrict place, BufferWritable& buf) const = 0;

    virtual void serialize_vec(const std::vector<AggregateDataPtr>& places, size_t offset,
                               BufferWritable& buf, const size_t num_rows) const = 0;

    virtual void serialize_to_column(const std::vector<AggregateDataPtr>& places, size_t offset,
                                     MutableColumnPtr& dst, const size_t num_rows) const = 0;

    virtual void serialize_without_key_to_column(ConstAggregateDataPtr __restrict place,
                                                 IColumn& to) const = 0;

    /// Deserializes state. This function is called only for empty (just created) states.
    virtual void deserialize(AggregateDataPtr __restrict place, BufferReadable& buf,
                             Arena&) const = 0;

    virtual void deserialize_vec(AggregateDataPtr places, const ColumnString* column, Arena&,
                                 size_t num_rows) const = 0;

    virtual void deserialize_and_merge_vec(const AggregateDataPtr* places, size_t offset,
                                           AggregateDataPtr rhs, const IColumn* column, Arena&,
                                           const size_t num_rows) const = 0;

    virtual void deserialize_and_merge_vec_selected(const AggregateDataPtr* places, size_t offset,
                                                    AggregateDataPtr rhs, const IColumn* column,
                                                    Arena&, const size_t num_rows) const = 0;

    virtual void deserialize_from_column(AggregateDataPtr places, const IColumn& column, Arena&,
                                         size_t num_rows) const = 0;

    /// Deserializes state and merge it with current aggregation function.
    virtual void deserialize_and_merge(AggregateDataPtr __restrict place,
                                       AggregateDataPtr __restrict rhs, BufferReadable& buf,
                                       Arena& arena) const = 0;

    virtual void deserialize_and_merge_from_column_range(AggregateDataPtr __restrict place,
                                                         const IColumn& column, size_t begin,
                                                         size_t end, Arena&) const = 0;

    virtual void deserialize_and_merge_from_column(AggregateDataPtr __restrict place,
                                                   const IColumn& column, Arena&) const = 0;

    /// Inserts results into a column.
    virtual void insert_result_into(ConstAggregateDataPtr __restrict place, IColumn& to) const = 0;

    virtual void insert_result_into_vec(const std::vector<AggregateDataPtr>& places,
                                        const size_t offset, IColumn& to,
                                        const size_t num_rows) const = 0;

    /** Contains a loop with calls to "add" function. You can collect arguments into array "places"
      *  and do a single call to "add_batch" for devirtualization and inlining.
      */
    virtual void add_batch(size_t batch_size, AggregateDataPtr* places, size_t place_offset,
                           const IColumn** columns, Arena&, bool agg_many = false) const = 0;

    // same as add_batch, but only call "add" function when place is not nullptr
    virtual void add_batch_selected(size_t batch_size, AggregateDataPtr* places,
                                    size_t place_offset, const IColumn** columns, Arena&) const = 0;

    /** The same for single place.
      */
    virtual void add_batch_single_place(size_t batch_size, AggregateDataPtr place,
                                        const IColumn** columns, Arena&) const = 0;

    // only used at agg reader
    virtual void add_batch_range(size_t batch_begin, size_t batch_end, AggregateDataPtr place,
                                 const IColumn** columns, Arena&, bool has_null = false) = 0;

    // only used at window function
    virtual void add_range_single_place(int64_t partition_start, int64_t partition_end,
                                        int64_t frame_start, int64_t frame_end,
                                        AggregateDataPtr place, const IColumn** columns,
                                        Arena& arena, UInt8* use_null_result,
                                        UInt8* could_use_previous_result) const = 0;

    virtual void streaming_agg_serialize(const IColumn** columns, BufferWritable& buf,
                                         const size_t num_rows, Arena&) const = 0;

    virtual void streaming_agg_serialize_to_column(const IColumn** columns, MutableColumnPtr& dst,
                                                   const size_t num_rows, Arena&) const = 0;

    const DataTypes& get_argument_types() const { return argument_types; }

    virtual MutableColumnPtr create_serialize_column() const { return ColumnString::create(); }

    virtual DataTypePtr get_serialized_type() const { return std::make_shared<DataTypeString>(); }

    virtual void set_version(const int version_) { version = version_; }

    virtual IAggregateFunction* transmit_to_stable() { return nullptr; }

    /// Verify function signature
    virtual Status verify_result_type(const bool without_key, const DataTypes& argument_types,
                                      const DataTypePtr result_type) const = 0;

    // agg function is used result column push_back to insert result,
    // and now want's resize column early and use operator[] to insert result.
    // but like result column is string column, it's can't resize dirctly with operator[]
    // need template specialization agg for the string type in insert_result_into_range
    virtual bool result_column_could_resize() const { return false; }

    virtual void insert_result_into_range(ConstAggregateDataPtr __restrict place, IColumn& to,
                                          const size_t start, const size_t end) const {
        for (size_t i = start; i < end; ++i) {
            insert_result_into(place, to);
        }
    }

    /// some agg function like sum/count/avg/min/max could support incremental mode,
    /// eg sum(col) over (rows between 3 preceding and 3 following), could resue the previous result
    /// sum[i] = sum[i-1] - col[x] + col[y]
    virtual bool supported_incremental_mode() const { return false; }

    /**
    * Executes the aggregate function in incremental mode.
    * This is a virtual function that should be overridden by aggregate functions supporting incremental calculation.
    * 
    * @param partition_start Start position of the current partition (inclusive)
    * @param partition_end End position of the current partition (exclusive)
    * @param frame_start Start position of the current window frame (inclusive)
    * @param frame_end End position of the current window frame (exclusive)
    * @param place Memory location to store aggregation results
    * @param columns Input columns for aggregation
    * @param arena Memory pool for allocations
    * @param previous_is_nul Whether previous value is NULL, if true, no need to subtract previous value
    * @param end_is_nul Whether the end boundary is NULL, if true, no need to add end value
    * @param has_null Whether the current column contains NULL values
    * @param use_null_result Output: whether to use NULL as result when the frame is empty
    * @param could_use_previous_result Output: whether previous result can be reused
    * @throws doris::Exception when called on a function that doesn't support incremental mode
    */
    virtual void execute_function_with_incremental(int64_t partition_start, int64_t partition_end,
                                                   int64_t frame_start, int64_t frame_end,
                                                   AggregateDataPtr place, const IColumn** columns,
                                                   Arena& arena, bool previous_is_nul,
                                                   bool end_is_nul, bool has_null,
                                                   UInt8* use_null_result,
                                                   UInt8* could_use_previous_result) const {
        throw doris::Exception(Status::FatalError(
                "Aggregate function " + get_name() +
                " does not support cumulative mode, but it is called in cumulative mode"));
    }

protected:
    DataTypes argument_types;
    int version {};
};

/// Implement method to obtain an address of 'add' function.
template <typename Derived>
class IAggregateFunctionHelper : public IAggregateFunction {
public:
    IAggregateFunctionHelper(const DataTypes& argument_types_)
            : IAggregateFunction(argument_types_) {}

    void destroy_vec(AggregateDataPtr __restrict place,
                     const size_t num_rows) const noexcept override {
        const size_t size_of_data_ = size_of_data();
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = 0; i != num_rows; ++i) {
            derived->destroy(place + size_of_data_ * i);
        }
    }

    void add_batch(size_t batch_size, AggregateDataPtr* places, size_t place_offset,
                   const IColumn** columns, Arena& arena, bool agg_many) const override {
        const Derived* derived = assert_cast<const Derived*>(this);

        if constexpr (std::is_same_v<Derived, AggregateFunctionBitmapCount<false, ColumnBitmap>> ||
                      std::is_same_v<Derived, AggregateFunctionBitmapCount<true, ColumnBitmap>> ||
                      std::is_same_v<Derived,
                                     AggregateFunctionBitmapOp<AggregateFunctionBitmapUnionOp>>) {
            if (agg_many) {
                flat_hash_map<AggregateDataPtr, std::vector<int>> place_rows;
                for (int i = 0; i < batch_size; ++i) {
                    auto iter = place_rows.find(places[i] + place_offset);
                    if (iter == place_rows.end()) {
                        std::vector<int> rows;
                        rows.push_back(i);
                        place_rows.emplace(places[i] + place_offset, rows);
                    } else {
                        iter->second.push_back(i);
                    }
                }
                auto iter = place_rows.begin();
                while (iter != place_rows.end()) {
                    derived->add_many(iter->first, columns, iter->second, arena);
                    iter++;
                }
                return;
            }
        }

        for (size_t i = 0; i < batch_size; ++i) {
            derived->add(places[i] + place_offset, columns, i, arena);
        }
    }

    void add_batch_selected(size_t batch_size, AggregateDataPtr* places, size_t place_offset,
                            const IColumn** columns, Arena& arena) const override {
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = 0; i < batch_size; ++i) {
            if (places[i]) {
                derived->add(places[i] + place_offset, columns, i, arena);
            }
        }
    }

    void add_batch_single_place(size_t batch_size, AggregateDataPtr place, const IColumn** columns,
                                Arena& arena) const override {
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = 0; i < batch_size; ++i) {
            derived->add(place, columns, i, arena);
        }
    }

    void add_range_single_place(int64_t partition_start, int64_t partition_end, int64_t frame_start,
                                int64_t frame_end, AggregateDataPtr place, const IColumn** columns,
                                Arena& arena, UInt8* use_null_result,
                                UInt8* could_use_previous_result) const override {
        const Derived* derived = assert_cast<const Derived*>(this);
        frame_start = std::max<int64_t>(frame_start, partition_start);
        frame_end = std::min<int64_t>(frame_end, partition_end);
        for (int64_t i = frame_start; i < frame_end; ++i) {
            derived->add(place, columns, i, arena);
        }
        if (frame_start >= frame_end) {
            if (!*could_use_previous_result) {
                *use_null_result = true;
            }
        } else {
            *use_null_result = false;
            *could_use_previous_result = true;
        }
    }

    void add_batch_range(size_t batch_begin, size_t batch_end, AggregateDataPtr place,
                         const IColumn** columns, Arena& arena, bool has_null) override {
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = batch_begin; i <= batch_end; ++i) {
            derived->add(place, columns, i, arena);
        }
    }

    void insert_result_into_vec(const std::vector<AggregateDataPtr>& places, const size_t offset,
                                IColumn& to, const size_t num_rows) const override {
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = 0; i != num_rows; ++i) {
            derived->insert_result_into(places[i] + offset, to);
        }
    }

    void serialize_vec(const std::vector<AggregateDataPtr>& places, size_t offset,
                       BufferWritable& buf, const size_t num_rows) const override {
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = 0; i != num_rows; ++i) {
            derived->serialize(places[i] + offset, buf);
            buf.commit();
        }
    }

    void serialize_to_column(const std::vector<AggregateDataPtr>& places, size_t offset,
                             MutableColumnPtr& dst, const size_t num_rows) const override {
        VectorBufferWriter writer(assert_cast<ColumnString&>(*dst));
        serialize_vec(places, offset, writer, num_rows);
    }

    void streaming_agg_serialize(const IColumn** columns, BufferWritable& buf,
                                 const size_t num_rows, Arena& arena) const override {
        std::vector<char> place(size_of_data());
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = 0; i != num_rows; ++i) {
            derived->create(place.data());
            DEFER({ derived->destroy(place.data()); });
            derived->add(place.data(), columns, i, arena);
            derived->serialize(place.data(), buf);
            buf.commit();
        }
    }

    void streaming_agg_serialize_to_column(const IColumn** columns, MutableColumnPtr& dst,
                                           const size_t num_rows, Arena& arena) const override {
        VectorBufferWriter writer(assert_cast<ColumnString&>(*dst));
        streaming_agg_serialize(columns, writer, num_rows, arena);
    }

    void serialize_without_key_to_column(ConstAggregateDataPtr __restrict place,
                                         IColumn& to) const override {
        VectorBufferWriter writter(assert_cast<ColumnString&>(to));
        assert_cast<const Derived*>(this)->serialize(place, writter);
        writter.commit();
    }

    void deserialize_vec(AggregateDataPtr places, const ColumnString* column, Arena& arena,
                         size_t num_rows) const override {
        const Derived* derived = assert_cast<const Derived*>(this);
        const auto size_of_data = derived->size_of_data();
        for (size_t i = 0; i != num_rows; ++i) {
            try {
                auto place = places + size_of_data * i;
                VectorBufferReader buffer_reader(column->get_data_at(i));
                derived->create(place);
                derived->deserialize(place, buffer_reader, arena);
            } catch (...) {
                for (int j = 0; j < i; ++j) {
                    auto place = places + size_of_data * j;
                    derived->destroy(place);
                }
                throw;
            }
        }
    }

    void deserialize_and_merge_vec(const AggregateDataPtr* places, size_t offset,
                                   AggregateDataPtr rhs, const IColumn* column, Arena& arena,
                                   const size_t num_rows) const override {
        const Derived* derived = assert_cast<const Derived*>(this);
        const auto size_of_data = derived->size_of_data();
        const auto* column_string = assert_cast<const ColumnString*>(column);

        for (size_t i = 0; i != num_rows; ++i) {
            try {
                auto rhs_place = rhs + size_of_data * i;
                VectorBufferReader buffer_reader(column_string->get_data_at(i));
                derived->create(rhs_place);
                derived->deserialize_and_merge(places[i] + offset, rhs_place, buffer_reader, arena);
            } catch (...) {
                for (int j = 0; j < i; ++j) {
                    auto place = rhs + size_of_data * j;
                    derived->destroy(place);
                }
                throw;
            }
        }

        derived->destroy_vec(rhs, num_rows);
    }

    void deserialize_and_merge_vec_selected(const AggregateDataPtr* places, size_t offset,
                                            AggregateDataPtr rhs, const IColumn* column,
                                            Arena& arena, const size_t num_rows) const override {
        const auto* derived = assert_cast<const Derived*>(this);
        const auto size_of_data = derived->size_of_data();
        const auto* column_string = assert_cast<const ColumnString*>(column);
        for (size_t i = 0; i != num_rows; ++i) {
            try {
                auto rhs_place = rhs + size_of_data * i;
                VectorBufferReader buffer_reader(column_string->get_data_at(i));
                derived->create(rhs_place);
                if (places[i]) {
                    derived->deserialize_and_merge(places[i] + offset, rhs_place, buffer_reader,
                                                   arena);
                }
            } catch (...) {
                for (int j = 0; j < i; ++j) {
                    auto place = rhs + size_of_data * j;
                    derived->destroy(place);
                }
                throw;
            }
        }
        derived->destroy_vec(rhs, num_rows);
    }

    void deserialize_from_column(AggregateDataPtr places, const IColumn& column, Arena& arena,
                                 size_t num_rows) const override {
        deserialize_vec(places, assert_cast<const ColumnString*>(&column), arena, num_rows);
    }

    void merge_vec(const AggregateDataPtr* places, size_t offset, ConstAggregateDataPtr rhs,
                   Arena& arena, const size_t num_rows) const override {
        const auto* derived = assert_cast<const Derived*>(this);
        const auto size_of_data = derived->size_of_data();
        for (size_t i = 0; i != num_rows; ++i) {
            derived->merge(places[i] + offset, rhs + size_of_data * i, arena);
        }
    }

    void merge_vec_selected(const AggregateDataPtr* places, size_t offset,
                            ConstAggregateDataPtr rhs, Arena& arena,
                            const size_t num_rows) const override {
        const auto* derived = assert_cast<const Derived*>(this);
        const auto size_of_data = derived->size_of_data();
        for (size_t i = 0; i != num_rows; ++i) {
            if (places[i]) {
                derived->merge(places[i] + offset, rhs + size_of_data * i, arena);
            }
        }
    }

    void deserialize_and_merge_from_column_range(AggregateDataPtr __restrict place,
                                                 const IColumn& column, size_t begin, size_t end,
                                                 Arena& arena) const override {
        DCHECK(end <= column.size() && begin <= end)
                << ", begin:" << begin << ", end:" << end << ", column.size():" << column.size();
        std::vector<char> deserialized_data(size_of_data());
        auto* deserialized_place = (AggregateDataPtr)deserialized_data.data();
        const ColumnString& column_string = assert_cast<const ColumnString&>(column);
        const Derived* derived = assert_cast<const Derived*>(this);
        for (size_t i = begin; i <= end; ++i) {
            VectorBufferReader buffer_reader(column_string.get_data_at(i));
            derived->create(deserialized_place);

            DEFER({ derived->destroy(deserialized_place); });

            derived->deserialize_and_merge(place, deserialized_place, buffer_reader, arena);
        }
    }

    void deserialize_and_merge_from_column(AggregateDataPtr __restrict place, const IColumn& column,
                                           Arena& arena) const override {
        if (column.empty()) {
            return;
        }
        deserialize_and_merge_from_column_range(place, column, 0, column.size() - 1, arena);
    }

    void deserialize_and_merge(AggregateDataPtr __restrict place, AggregateDataPtr __restrict rhs,
                               BufferReadable& buf, Arena& arena) const override {
        assert_cast<const Derived*, TypeCheckOnRelease::DISABLE>(this)->deserialize(rhs, buf,
                                                                                    arena);
        assert_cast<const Derived*, TypeCheckOnRelease::DISABLE>(this)->merge(place, rhs, arena);
    }

    Status verify_result_type(const bool without_key, const DataTypes& argument_types_with_nullable,
                              const DataTypePtr result_type_with_nullable) const override {
        DataTypePtr function_result_type = assert_cast<const Derived*>(this)->get_return_type();

        if (function_result_type->equals(*result_type_with_nullable)) {
            return Status::OK();
        }

        if (!remove_nullable(function_result_type)
                     ->equals(*remove_nullable(result_type_with_nullable))) {
            return Status::InternalError(
                    "Result type of {} is not matched, planner expect {}, but get {}, with group "
                    "by: "
                    "{}",
                    get_name(), result_type_with_nullable->get_name(),
                    function_result_type->get_name(), !without_key);
        }

        if (without_key == true) {
            if (result_type_with_nullable->is_nullable()) {
                // This branch is decicated for NullableAggregateFunction.
                // When they are executed without group by key, the result from planner will be AlwaysNullable
                // since Planer does not know whether there are any invalid input at runtime, if so, the result
                // should be Null, so the result type must be nullable.
                // Backend will wrap a ColumnNullable in this situation. For example: AggLocalState::_get_without_key_result
                return Status::OK();
            }
        }

        // Executed with group by key, result type must be exactly same with the return type from Planner.
        return Status::InternalError(
                "Result type of {} is not matched, planner expect {}, but get {}, with group by: "
                "{}",
                get_name(), result_type_with_nullable->get_name(), function_result_type->get_name(),
                !without_key);
    }
};

/// Implements several methods for manipulation with data. T - type of structure with data for aggregation.
template <typename T, typename Derived, bool create_with_argument_types = false>
class IAggregateFunctionDataHelper : public IAggregateFunctionHelper<Derived> {
protected:
    using Data = T;

    static Data& data(AggregateDataPtr __restrict place) { return *reinterpret_cast<Data*>(place); }
    static const Data& data(ConstAggregateDataPtr __restrict place) {
        return *reinterpret_cast<const Data*>(place);
    }

public:
    IAggregateFunctionDataHelper(const DataTypes& argument_types_)
            : IAggregateFunctionHelper<Derived>(argument_types_) {}

    void create(AggregateDataPtr __restrict place) const override {
        if constexpr (create_with_argument_types) {
            new (place) Data(IAggregateFunction::argument_types);
        } else {
            new (place) Data;
        }
    }

    void destroy(AggregateDataPtr __restrict place) const noexcept override { data(place).~Data(); }

    bool has_trivial_destructor() const override { return std::is_trivially_destructible_v<Data>; }

    size_t size_of_data() const override { return sizeof(Data); }

    /// NOTE: Currently not used (structures with aggregation state are put without alignment).
    size_t align_of_data() const override { return alignof(Data); }

    void reset(AggregateDataPtr place) const override {
        destroy(place);
        create(place);
    }

    void deserialize_and_merge(AggregateDataPtr __restrict place, AggregateDataPtr __restrict rhs,
                               BufferReadable& buf, Arena& arena) const override {
        assert_cast<const Derived*, TypeCheckOnRelease::DISABLE>(this)->deserialize(rhs, buf,
                                                                                    arena);
        assert_cast<const Derived*, TypeCheckOnRelease::DISABLE>(this)->merge(place, rhs, arena);
    }
};

class AggregateFunctionGuard {
public:
    using AggregateData = std::remove_pointer_t<AggregateDataPtr>;

    explicit AggregateFunctionGuard(const IAggregateFunction* function)
            : _function(function),
              _data(std::make_unique<AggregateData[]>(function->size_of_data())) {
        _function->create(_data.get());
    }
    ~AggregateFunctionGuard() { _function->destroy(_data.get()); }
    AggregateDataPtr data() { return _data.get(); }

private:
    const IAggregateFunction* _function;
    std::unique_ptr<AggregateData[]> _data;
};

} // namespace doris::vectorized

#include "common/compile_check_end.h"
