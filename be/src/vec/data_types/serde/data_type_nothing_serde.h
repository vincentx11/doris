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

#include <glog/logging.h>
#include <stdint.h>

#include <ostream>

#include "common/exception.h"
#include "common/status.h"
#include "data_type_serde.h"

namespace doris {
class PValues;
struct JsonbValue;

namespace vectorized {
class IColumn;
class Arena;

class DataTypeNothingSerde : public DataTypeSerDe {
public:
    DataTypeNothingSerde() = default;

    std::string get_name() const override { return "Nothing"; }

    Status serialize_one_cell_to_json(const IColumn& column, int64_t row_num, BufferWritable& bw,
                                      FormatOptions& options) const override {
        return Status::NotSupported("serialize_one_cell_to_json with type " + column.get_name());
    }

    Status serialize_column_to_json(const IColumn& column, int64_t start_idx, int64_t end_idx,
                                    BufferWritable& bw, FormatOptions& options) const override {
        return Status::NotSupported("serialize_column_to_json with type [{}]", column.get_name());
    }
    Status deserialize_one_cell_from_json(IColumn& column, Slice& slice,
                                          const FormatOptions& options) const override {
        return Status::NotSupported("deserialize_one_cell_from_text with type " +
                                    column.get_name());
    }
    Status deserialize_column_from_json_vector(IColumn& column, std::vector<Slice>& slices,
                                               uint64_t* num_deserialized,
                                               const FormatOptions& options) const override {
        return Status::NotSupported("deserialize_column_from_text_vector with type " +
                                    column.get_name());
    }
    Status write_one_cell_to_json(const IColumn& column, rapidjson::Value& result,
                                  rapidjson::Document::AllocatorType& allocator, Arena& mem_pool,
                                  int64_t row_num) const override {
        result.SetNull();
        return Status::OK();
    }

    Status read_one_cell_from_json(IColumn& column, const rapidjson::Value& result) const override {
        if (result.IsNull()) {
            column.insert_default();
        }
        return Status::OK();
    }

    Status write_column_to_pb(const IColumn& column, PValues& result, int64_t start,
                              int64_t end) const override {
        return Status::NotSupported("write_column_to_pb with type " + column.get_name());
    }
    Status read_column_from_pb(IColumn& column, const PValues& arg) const override {
        return Status::NotSupported("read_column_from_pb with type " + column.get_name());
    }
    void write_one_cell_to_jsonb(const IColumn& column, JsonbWriter& result, Arena& mem_pool,
                                 int32_t col_id, int64_t row_num) const override {
        throw doris::Exception(ErrorCode::NOT_IMPLEMENTED_ERROR,
                               "write_one_cell_to_jsonb with type " + column.get_name());
    }

    void read_one_cell_from_jsonb(IColumn& column, const JsonbValue* arg) const override {
        throw doris::Exception(ErrorCode::NOT_IMPLEMENTED_ERROR,
                               "read_one_cell_from_jsonb with type " + column.get_name());
    }

    Status write_column_to_arrow(const IColumn& column, const NullMap* null_map,
                                 arrow::ArrayBuilder* array_builder, int64_t start, int64_t end,
                                 const cctz::time_zone& ctz) const override {
        return Status::Error(ErrorCode::NOT_IMPLEMENTED_ERROR,
                             "write_column_to_arrow with type " + column.get_name());
    }
    Status read_column_from_arrow(IColumn& column, const arrow::Array* arrow_array, int64_t start,
                                  int64_t end, const cctz::time_zone& ctz) const override {
        return Status::Error(ErrorCode::NOT_IMPLEMENTED_ERROR,
                             "read_column_from_arrow with type " + column.get_name());
    }

    Status write_column_to_mysql(const IColumn& column, MysqlRowBuffer<true>& row_buffer,
                                 int64_t row_idx, bool col_const,
                                 const FormatOptions& options) const override {
        return Status::NotSupported("write_column_to_mysql with type " + column.get_name());
    }

    Status write_column_to_mysql(const IColumn& column, MysqlRowBuffer<false>& row_buffer,
                                 int64_t row_idx, bool col_const,
                                 const FormatOptions& options) const override {
        return Status::NotSupported("write_column_to_mysql with type " + column.get_name());
    }

    Status write_column_to_orc(const std::string& timezone, const IColumn& column,
                               const NullMap* null_map, orc::ColumnVectorBatch* orc_col_batch,
                               int64_t start, int64_t end,
                               vectorized::Arena& arena) const override {
        return Status::NotSupported("write_column_to_orc with type " + column.get_name());
    }
};
} // namespace vectorized
} // namespace doris
