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

#include "vec/sink/vtablet_block_convertor.h"

#include <fmt/format.h>
#include <gen_cpp/FrontendService.h>
#include <glog/logging.h>
#include <google/protobuf/stubs/common.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "common/compiler_util.h" // IWYU pragma: keep
#include "common/consts.h"
#include "common/status.h"
#include "runtime/descriptors.h"
#include "runtime/runtime_state.h"
#include "service/brpc.h"
#include "util/binary_cast.hpp"
#include "util/brpc_client_cache.h"
#include "util/thread.h"
#include "vec/columns/column.h"
#include "vec/columns/column_array.h"
#include "vec/columns/column_const.h"
#include "vec/columns/column_decimal.h"
#include "vec/columns/column_map.h"
#include "vec/columns/column_nullable.h"
#include "vec/columns/column_string.h"
#include "vec/columns/column_struct.h"
#include "vec/common/assert_cast.h"
#include "vec/core/block.h"
#include "vec/core/types.h"
#include "vec/core/wide_integer_to_string.h"
#include "vec/data_types/data_type.h"
#include "vec/data_types/data_type_array.h"
#include "vec/data_types/data_type_decimal.h"
#include "vec/data_types/data_type_map.h"
#include "vec/data_types/data_type_nullable.h"
#include "vec/data_types/data_type_struct.h"
#include "vec/exprs/vexpr.h"
#include "vec/exprs/vexpr_context.h"
#include "vec/functions/function_helpers.h"

namespace doris::vectorized {
#include "common/compile_check_begin.h"

Status OlapTableBlockConvertor::validate_and_convert_block(
        RuntimeState* state, vectorized::Block* input_block,
        std::shared_ptr<vectorized::Block>& block, vectorized::VExprContextSPtrs output_vexpr_ctxs,
        size_t rows, bool& has_filtered_rows) {
    DCHECK(input_block->rows() > 0);

    block = vectorized::Block::create_shared(input_block->get_columns_with_type_and_name());
    if (!output_vexpr_ctxs.empty()) {
        // Do vectorized expr here to speed up load
        RETURN_IF_ERROR(vectorized::VExprContext::get_output_block_after_execute_exprs(
                output_vexpr_ctxs, *input_block, block.get()));
    }

    if (_is_partial_update_and_auto_inc) {
        // If this load is partial update and this table has a auto inc column,
        // e.g. table schema: k1, v1, v2(auto inc)
        // 1. insert columns include auto inc column
        // e.g. insert into table (k1, v2) value(a, 1);
        // we do nothing.
        // 2. insert columns do not include auto inc column
        // e.g. insert into table (k1, v1) value(a, a);
        // we need to fill auto_inc_cols by creating a new column.
        if (!_auto_inc_col_idx.has_value()) {
            RETURN_IF_ERROR(_partial_update_fill_auto_inc_cols(block.get(), rows));
        }
    } else if (_auto_inc_col_idx.has_value()) {
        // fill the valus for auto-increment columns
        DCHECK_EQ(_is_partial_update_and_auto_inc, false);
        RETURN_IF_ERROR(_fill_auto_inc_cols(block.get(), rows));
    }

    int filtered_rows = 0;
    {
        SCOPED_RAW_TIMER(&_validate_data_ns);
        _filter_map.clear();
        _filter_map.resize(rows, 0);
        auto st = _validate_data(state, block.get(), rows, filtered_rows);
        _num_filtered_rows += filtered_rows;
        has_filtered_rows = filtered_rows > 0;
        if (!st.ok()) {
            return st;
        }
        _convert_to_dest_desc_block(block.get());
    }

    return Status::OK();
}

void OlapTableBlockConvertor::init_autoinc_info(int64_t db_id, int64_t table_id, int batch_size,
                                                bool is_partial_update_and_auto_inc,
                                                int32_t auto_increment_column_unique_id) {
    _batch_size = batch_size;
    if (is_partial_update_and_auto_inc) {
        _is_partial_update_and_auto_inc = is_partial_update_and_auto_inc;
        _auto_inc_id_buffer = GlobalAutoIncBuffers::GetInstance()->get_auto_inc_buffer(
                db_id, table_id, auto_increment_column_unique_id);
        return;
    }
    for (size_t idx = 0; idx < _output_tuple_desc->slots().size(); idx++) {
        if (_output_tuple_desc->slots()[idx]->is_auto_increment()) {
            _auto_inc_col_idx = idx;
            _auto_inc_id_buffer = GlobalAutoIncBuffers::GetInstance()->get_auto_inc_buffer(
                    db_id, table_id, _output_tuple_desc->slots()[idx]->col_unique_id());
            _auto_inc_id_buffer->set_batch_size_at_least(_batch_size);
            break;
        }
    }
}

template <bool is_min>
DecimalV2Value OlapTableBlockConvertor::_get_decimalv2_min_or_max(const DataTypePtr& type) {
    std::map<std::pair<int, int>, DecimalV2Value>* pmap;
    if constexpr (is_min) {
        pmap = &_min_decimalv2_val;
    } else {
        pmap = &_max_decimalv2_val;
    }

    // found
    auto iter = pmap->find(
            {remove_nullable(type)->get_precision(), remove_nullable(type)->get_scale()});
    if (iter != pmap->end()) {
        return iter->second;
    }

    // save min or max DecimalV2Value for next time
    DecimalV2Value value;
    if constexpr (is_min) {
        value.to_min_decimal(type->get_precision(), type->get_scale());
    } else {
        value.to_max_decimal(type->get_precision(), type->get_scale());
    }
    pmap->emplace(std::pair<int, int> {type->get_precision(), type->get_scale()}, value);
    return value;
}

template <typename DecimalType, bool IsMin>
DecimalType OlapTableBlockConvertor::_get_decimalv3_min_or_max(const DataTypePtr& type) {
    std::map<int, typename DecimalType::NativeType>* pmap;
    if constexpr (std::is_same_v<DecimalType, vectorized::Decimal32>) {
        pmap = IsMin ? &_min_decimal32_val : &_max_decimal32_val;
    } else if constexpr (std::is_same_v<DecimalType, vectorized::Decimal64>) {
        pmap = IsMin ? &_min_decimal64_val : &_max_decimal64_val;
    } else if constexpr (std::is_same_v<DecimalType, vectorized::Decimal128V3>) {
        pmap = IsMin ? &_min_decimal128_val : &_max_decimal128_val;
    } else {
        pmap = IsMin ? &_min_decimal256_val : &_max_decimal256_val;
    }

    // found
    auto iter = pmap->find(type->get_precision());
    if (iter != pmap->end()) {
        return DecimalType(iter->second);
    }

    typename DecimalType::NativeType value;
    if constexpr (IsMin) {
        value = vectorized::min_decimal_value<DecimalType::PType>(type->get_precision());
    } else {
        value = vectorized::max_decimal_value<DecimalType::PType>(type->get_precision());
    }
    pmap->emplace(type->get_precision(), value);
    return DecimalType(value);
}

Status OlapTableBlockConvertor::_internal_validate_column(
        RuntimeState* state, const DataTypePtr& type, vectorized::ColumnPtr column,
        size_t slot_index, fmt::memory_buffer& error_prefix, const size_t row_count,
        vectorized::IColumn::Permutation* rows) {
    DCHECK((rows == nullptr) || (rows->size() == row_count));
    fmt::memory_buffer error_msg;
    auto set_invalid_and_append_error_msg = [&](size_t row) {
        _filter_map[row] = true;
        auto ret = state->append_error_msg_to_file([]() -> std::string { return ""; },
                                                   [&error_prefix, &error_msg]() -> std::string {
                                                       return fmt::to_string(error_prefix) +
                                                              fmt::to_string(error_msg);
                                                   });
        error_msg.clear();
        return ret;
    };

    auto column_ptr = vectorized::check_and_get_column<vectorized::ColumnNullable>(*column);
    auto& real_column_ptr = column_ptr == nullptr ? column : (column_ptr->get_nested_column_ptr());
    auto null_map = column_ptr == nullptr ? nullptr : column_ptr->get_null_map_data().data();
    auto need_to_validate = [&null_map, this](size_t j, size_t row) {
        return !_filter_map[row] && (null_map == nullptr || null_map[j] == 0);
    };

    auto string_column_checker = [&](const ColumnString* column_string) {
        int limit = config::string_type_length_soft_limit_bytes;
        int len = -1;
        // when type.len is negative, std::min will return overflow value, so we need to check it
        if (const auto* type_str =
                    check_and_get_data_type<DataTypeString>(remove_nullable(type).get())) {
            if (type_str->len() >= 0) {
                len = type_str->len();
                limit = std::min(limit, type_str->len());
            }
        }

        auto* __restrict offsets = column_string->get_offsets().data();
        int invalid_count = 0;
        for (int64_t j = 0; j < row_count; ++j) {
            invalid_count += (offsets[j] - offsets[j - 1]) > limit;
        }

        if (invalid_count) {
            for (size_t j = 0; j < row_count; ++j) {
                auto row = rows ? (*rows)[j] : j;
                if (need_to_validate(j, row)) {
                    auto str_val = column_string->get_data_at(j);
                    bool invalid = str_val.size > limit;
                    if (invalid) {
                        if (str_val.size > len) {
                            fmt::format_to(error_msg, "{}",
                                           "the length of input is too long than schema. ");
                            fmt::format_to(error_msg, "first 32 bytes of input str: [{}] ",
                                           str_val.to_prefix(32));
                            fmt::format_to(error_msg, "schema length: {}; ", len);
                            fmt::format_to(error_msg, "actual length: {}; ", str_val.size);
                        } else if (str_val.size > limit) {
                            fmt::format_to(
                                    error_msg, "{}",
                                    "the length of input string is too long than vec schema. ");
                            fmt::format_to(error_msg, "first 32 bytes of input str: [{}] ",
                                           str_val.to_prefix(32));
                            fmt::format_to(error_msg, "schema length: {}; ", len);
                            fmt::format_to(error_msg, "limit length: {}; ", limit);
                            fmt::format_to(error_msg, "actual length: {}; ", str_val.size);
                        }
                        RETURN_IF_ERROR(set_invalid_and_append_error_msg(row));
                    }
                }
            }
        }
        return Status::OK();
    };

    switch (type->get_primitive_type()) {
    case TYPE_CHAR:
    case TYPE_VARCHAR:
    case TYPE_STRING: {
        const auto column_string =
                assert_cast<const vectorized::ColumnString*>(real_column_ptr.get());
        RETURN_IF_ERROR(string_column_checker(column_string));
        break;
    }
    case TYPE_JSONB: {
        const auto* column_string =
                assert_cast<const vectorized::ColumnString*>(real_column_ptr.get());
        for (size_t j = 0; j < row_count; ++j) {
            if (!_filter_map[j]) {
                if (type->is_nullable() && column_ptr && column_ptr->is_null_at(j)) {
                    continue;
                }
                auto str_val = column_string->get_data_at(j);
                bool invalid = str_val.size == 0;
                if (invalid) {
                    error_msg.clear();
                    fmt::format_to(error_msg, "{}", "jsonb with size 0 is invalid");
                    RETURN_IF_ERROR(set_invalid_and_append_error_msg(j));
                }
            }
        }
        break;
    }
    case TYPE_DECIMALV2: {
        auto* column_decimal = const_cast<vectorized::ColumnDecimal128V2*>(
                assert_cast<const vectorized::ColumnDecimal128V2*>(real_column_ptr.get()));
        const auto& max_decimalv2 = _get_decimalv2_min_or_max<false>(type);
        const auto& min_decimalv2 = _get_decimalv2_min_or_max<true>(type);
        for (size_t j = 0; j < row_count; ++j) {
            auto row = rows ? (*rows)[j] : j;
            if (need_to_validate(j, row)) {
                auto dec_val = binary_cast<vectorized::Int128, DecimalV2Value>(
                        column_decimal->get_data()[j]);
                bool invalid = false;

                if (dec_val.greater_than_scale(type->get_scale())) {
                    auto code =
                            dec_val.round(&dec_val, remove_nullable(type)->get_scale(), HALF_UP);
                    column_decimal->get_data()[j] = dec_val.value();

                    if (code != E_DEC_OK) {
                        fmt::format_to(error_msg, "round one decimal failed.value={}; ",
                                       dec_val.to_string());
                        invalid = true;
                    }
                }
                if (dec_val > max_decimalv2 || dec_val < min_decimalv2) {
                    fmt::format_to(error_msg, "{}", "decimal value is not valid for definition");
                    fmt::format_to(error_msg, ", value={}", dec_val.to_string());
                    fmt::format_to(error_msg, ", precision={}, scale={}", type->get_precision(),
                                   type->get_scale());
                    fmt::format_to(error_msg, ", min={}, max={}; ", min_decimalv2.to_string(),
                                   max_decimalv2.to_string());
                    invalid = true;
                }

                if (invalid) {
                    RETURN_IF_ERROR(set_invalid_and_append_error_msg(row));
                }
            }
        }
        break;
    }
    case TYPE_DECIMAL32: {
#define CHECK_VALIDATION_FOR_DECIMALV3(DecimalType)                                               \
    auto column_decimal = const_cast<vectorized::ColumnDecimal<DecimalType::PType>*>(             \
            assert_cast<const vectorized::ColumnDecimal<DecimalType::PType>*>(                    \
                    real_column_ptr.get()));                                                      \
    const auto& max_decimal = _get_decimalv3_min_or_max<DecimalType, false>(type);                \
    const auto& min_decimal = _get_decimalv3_min_or_max<DecimalType, true>(type);                 \
    const auto* __restrict datas = column_decimal->get_data().data();                             \
    int invalid_count = 0;                                                                        \
    for (int j = 0; j < row_count; ++j) {                                                         \
        const auto dec_val = datas[j];                                                            \
        invalid_count += dec_val > max_decimal || dec_val < min_decimal;                          \
    }                                                                                             \
    if (invalid_count) {                                                                          \
        for (size_t j = 0; j < row_count; ++j) {                                                  \
            auto row = rows ? (*rows)[j] : j;                                                     \
            if (need_to_validate(j, row)) {                                                       \
                auto dec_val = column_decimal->get_data()[j];                                     \
                bool invalid = false;                                                             \
                if (dec_val > max_decimal || dec_val < min_decimal) {                             \
                    fmt::format_to(error_msg, "{}", "decimal value is not valid for definition"); \
                    fmt::format_to(error_msg, ", value={}", dec_val.value);                       \
                    fmt::format_to(error_msg, ", precision={}, scale={}", type->get_precision(),  \
                                   type->get_scale());                                            \
                    fmt::format_to(error_msg, ", min={}, max={}; ", min_decimal.value,            \
                                   max_decimal.value);                                            \
                    invalid = true;                                                               \
                }                                                                                 \
                if (invalid) {                                                                    \
                    RETURN_IF_ERROR(set_invalid_and_append_error_msg(row));                       \
                }                                                                                 \
            }                                                                                     \
        }                                                                                         \
    }
        CHECK_VALIDATION_FOR_DECIMALV3(vectorized::Decimal32);
        break;
    }
    case TYPE_DECIMAL64: {
        CHECK_VALIDATION_FOR_DECIMALV3(vectorized::Decimal64);
        break;
    }
    case TYPE_DECIMAL128I: {
        CHECK_VALIDATION_FOR_DECIMALV3(vectorized::Decimal128V3);
        break;
    }
    case TYPE_DECIMAL256: {
        CHECK_VALIDATION_FOR_DECIMALV3(vectorized::Decimal256);
        break;
    }
#undef CHECK_VALIDATION_FOR_DECIMALV3
    case TYPE_ARRAY: {
        const auto* column_array =
                assert_cast<const vectorized::ColumnArray*>(real_column_ptr.get());
        const auto* type_array =
                assert_cast<const vectorized::DataTypeArray*>(remove_nullable(type).get());
        auto nested_type = type_array->get_nested_type();
        const auto& offsets = column_array->get_offsets();
        vectorized::IColumn::Permutation permutation(offsets.back());
        for (size_t r = 0; r < row_count; ++r) {
            for (size_t c = offsets[r - 1]; c < offsets[r]; ++c) {
                permutation[c] = rows ? (*rows)[r] : r;
            }
        }
        fmt::format_to(error_prefix, "ARRAY type failed: ");
        RETURN_IF_ERROR(_validate_column(state, nested_type, column_array->get_data_ptr(),
                                         slot_index, error_prefix, permutation.size(),
                                         &permutation));
        break;
    }
    case TYPE_MAP: {
        const auto column_map = assert_cast<const vectorized::ColumnMap*>(real_column_ptr.get());
        const auto* type_map =
                assert_cast<const vectorized::DataTypeMap*>(remove_nullable(type).get());
        auto key_type = type_map->get_key_type();
        auto val_type = type_map->get_value_type();
        const auto& offsets = column_map->get_offsets();
        vectorized::IColumn::Permutation permutation(offsets.back());
        for (size_t r = 0; r < row_count; ++r) {
            for (size_t c = offsets[r - 1]; c < offsets[r]; ++c) {
                permutation[c] = rows ? (*rows)[r] : r;
            }
        }
        fmt::format_to(error_prefix, "MAP type failed: ");
        RETURN_IF_ERROR(_validate_column(state, key_type, column_map->get_keys_ptr(), slot_index,
                                         error_prefix, permutation.size(), &permutation));
        RETURN_IF_ERROR(_validate_column(state, val_type, column_map->get_values_ptr(), slot_index,
                                         error_prefix, permutation.size(), &permutation));
        break;
    }
    case TYPE_STRUCT: {
        const auto column_struct =
                assert_cast<const vectorized::ColumnStruct*>(real_column_ptr.get());
        const auto* type_struct =
                assert_cast<const vectorized::DataTypeStruct*>(remove_nullable(type).get());
        DCHECK(type_struct->get_elements().size() == column_struct->tuple_size());
        fmt::format_to(error_prefix, "STRUCT type failed: ");
        for (size_t sc = 0; sc < column_struct->tuple_size(); ++sc) {
            RETURN_IF_ERROR(_validate_column(
                    state, type_struct->get_element(sc), column_struct->get_column_ptr(sc),
                    slot_index, error_prefix, column_struct->get_column_ptr(sc)->size()));
        }
        break;
    }
    case TYPE_AGG_STATE: {
        auto* column_string = vectorized::check_and_get_column<ColumnString>(*real_column_ptr);
        if (column_string) {
            RETURN_IF_ERROR(string_column_checker(column_string));
        }
        break;
    }
    default:
        break;
    }

    // Dispose the column should do not contain the NULL value
    // Only two case:
    // 1. column is nullable but the desc is not nullable
    // 2. desc->type is BITMAP
    if ((!type->is_nullable() || type->get_primitive_type() == TYPE_BITMAP) && column_ptr) {
        for (int j = 0; j < row_count; ++j) {
            auto row = rows ? (*rows)[j] : j;
            if (null_map[j] && !_filter_map[row]) {
                fmt::format_to(error_msg, "null value for not null column, type={}",
                               type->get_name());
                RETURN_IF_ERROR(set_invalid_and_append_error_msg(row));
            }
        }
    }

    return Status::OK();
}

Status OlapTableBlockConvertor::_validate_data(RuntimeState* state, vectorized::Block* block,
                                               const size_t rows, int& filtered_rows) {
    filtered_rows = 0;
    Defer defer {[&] {
        for (int i = 0; i < rows; ++i) {
            filtered_rows += _filter_map[i];
        }
    }};
    for (int i = 0; i < _output_tuple_desc->slots().size(); ++i) {
        SlotDescriptor* desc = _output_tuple_desc->slots()[i];
        block->get_by_position(i).column =
                block->get_by_position(i).column->convert_to_full_column_if_const();
        const auto& column = block->get_by_position(i).column;

        fmt::memory_buffer error_prefix;
        fmt::format_to(error_prefix, "column_name[{}], ", desc->col_name());
        RETURN_IF_ERROR(_validate_column(state, desc->type(), column, i, error_prefix, rows));
    }
    return Status::OK();
}

void OlapTableBlockConvertor::_convert_to_dest_desc_block(doris::vectorized::Block* block) {
    for (int i = 0; i < _output_tuple_desc->slots().size() && i < block->columns(); ++i) {
        SlotDescriptor* desc = _output_tuple_desc->slots()[i];
        if (desc->is_nullable() != block->get_by_position(i).type->is_nullable()) {
            if (desc->is_nullable()) {
                block->get_by_position(i).type =
                        vectorized::make_nullable(block->get_by_position(i).type);
                block->get_by_position(i).column =
                        vectorized::make_nullable(block->get_by_position(i).column);
            } else {
                block->get_by_position(i).type = assert_cast<const vectorized::DataTypeNullable&>(
                                                         *block->get_by_position(i).type)
                                                         .get_nested_type();
                block->get_by_position(i).column = assert_cast<const vectorized::ColumnNullable&>(
                                                           *block->get_by_position(i).column)
                                                           .get_nested_column_ptr();
            }
        }
    }
}

Status OlapTableBlockConvertor::_fill_auto_inc_cols(vectorized::Block* block, size_t rows) {
    size_t idx = _auto_inc_col_idx.value();
    SlotDescriptor* slot = _output_tuple_desc->slots()[idx];
    DCHECK(slot->type()->get_primitive_type() == PrimitiveType::TYPE_BIGINT);
    DCHECK(!slot->is_nullable());

    size_t null_value_count = 0;
    auto dst_column = vectorized::ColumnInt64::create();
    vectorized::ColumnInt64::Container& dst_values = dst_column->get_data();

    vectorized::ColumnPtr src_column_ptr = block->get_by_position(idx).column;
    if (const auto* const_column =
                check_and_get_column<vectorized::ColumnConst>(src_column_ptr.get())) {
        // for insert stmt like "insert into tbl1 select null,col1,col2,... from tbl2" or
        // "insert into tbl1 select 1,col1,col2,... from tbl2", the type of literal's column
        // will be `ColumnConst`
        if (const_column->is_null_at(0)) {
            // the input of autoinc column are all null literals
            // fill the column with generated ids
            null_value_count = rows;
            std::vector<std::pair<int64_t, size_t>> res;
            RETURN_IF_ERROR(_auto_inc_id_buffer->sync_request_ids(null_value_count, &res));
            for (auto [start, length] : res) {
                _auto_inc_id_allocator.insert_ids(start, length);
            }

            for (size_t i = 0; i < rows; i++) {
                dst_values.emplace_back(_auto_inc_id_allocator.next_id());
            }
        } else {
            // the input of autoinc column are all int64 literals
            // fill the column with that literal
            int64_t value = const_column->get_int(0);
            dst_values.resize_fill(rows, value);
        }
    } else if (const auto* src_nullable_column =
                       check_and_get_column<vectorized::ColumnNullable>(src_column_ptr.get())) {
        auto src_nested_column_ptr = src_nullable_column->get_nested_column_ptr();
        const auto& null_map_data = src_nullable_column->get_null_map_data();
        dst_values.reserve(rows);
        for (size_t i = 0; i < rows; i++) {
            null_value_count += null_map_data[i];
        }
        std::vector<std::pair<int64_t, size_t>> res;
        RETURN_IF_ERROR(_auto_inc_id_buffer->sync_request_ids(null_value_count, &res));
        for (auto [start, length] : res) {
            _auto_inc_id_allocator.insert_ids(start, length);
        }

        for (size_t i = 0; i < rows; i++) {
            dst_values.emplace_back((null_map_data[i] != 0) ? _auto_inc_id_allocator.next_id()
                                                            : src_nested_column_ptr->get_int(i));
        }
    } else {
        return Status::OK();
    }
    block->get_by_position(idx).column = std::move(dst_column);
    block->get_by_position(idx).type = remove_nullable(slot->type());
    return Status::OK();
}

Status OlapTableBlockConvertor::_partial_update_fill_auto_inc_cols(vectorized::Block* block,
                                                                   size_t rows) {
    // avoid duplicate PARTIAL_UPDATE_AUTO_INC_COL
    if (block->has(BeConsts::PARTIAL_UPDATE_AUTO_INC_COL)) {
        return Status::OK();
    }
    auto dst_column = vectorized::ColumnInt64::create();
    vectorized::ColumnInt64::Container& dst_values = dst_column->get_data();
    size_t null_value_count = rows;
    std::vector<std::pair<int64_t, size_t>> res;
    RETURN_IF_ERROR(_auto_inc_id_buffer->sync_request_ids(null_value_count, &res));
    for (auto [start, length] : res) {
        _auto_inc_id_allocator.insert_ids(start, length);
    }

    for (size_t i = 0; i < rows; i++) {
        dst_values.emplace_back(_auto_inc_id_allocator.next_id());
    }
    block->insert(vectorized::ColumnWithTypeAndName(std::move(dst_column),
                                                    std::make_shared<DataTypeInt64>(),
                                                    BeConsts::PARTIAL_UPDATE_AUTO_INC_COL));
    return Status::OK();
}

} // namespace doris::vectorized
