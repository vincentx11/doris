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

#include "vec/exprs/vectorized_agg_fn.h"

#include <fmt/format.h>
#include <fmt/ranges.h> // IWYU pragma: keep
#include <gen_cpp/Exprs_types.h>
#include <gen_cpp/PlanNodes_types.h>
#include <glog/logging.h>

#include <memory>
#include <ostream>
#include <string_view>

#include "common/config.h"
#include "common/object_pool.h"
#include "vec/aggregate_functions/aggregate_function_java_udaf.h"
#include "vec/aggregate_functions/aggregate_function_rpc.h"
#include "vec/aggregate_functions/aggregate_function_simple_factory.h"
#include "vec/aggregate_functions/aggregate_function_sort.h"
#include "vec/aggregate_functions/aggregate_function_state_merge.h"
#include "vec/aggregate_functions/aggregate_function_state_union.h"
#include "vec/core/block.h"
#include "vec/core/column_with_type_and_name.h"
#include "vec/core/materialize_block.h"
#include "vec/data_types/data_type_agg_state.h"
#include "vec/data_types/data_type_factory.hpp"
#include "vec/exprs/vexpr.h"
#include "vec/exprs/vexpr_context.h"
#include "vec/utils/util.hpp"

static constexpr int64_t BE_VERSION_THAT_SUPPORT_NULLABLE_CHECK = 8;

namespace doris {
class RowDescriptor;
namespace vectorized {
class Arena;
class BufferWritable;
class IColumn;
} // namespace vectorized
} // namespace doris

namespace doris::vectorized {
#include "common/compile_check_begin.h"

template <class FunctionType>
AggregateFunctionPtr get_agg_state_function(const DataTypes& argument_types,
                                            DataTypePtr return_type) {
    return FunctionType::create(
            assert_cast<const DataTypeAggState*>(argument_types[0].get())->get_nested_function(),
            argument_types, return_type);
}

AggFnEvaluator::AggFnEvaluator(const TExprNode& desc, const bool without_key,
                               const bool is_window_function)
        : _fn(desc.fn),
          _is_merge(desc.agg_expr.is_merge_agg),
          _without_key(without_key),
          _is_window_function(is_window_function),
          _data_type(DataTypeFactory::instance().create_data_type(
                  desc.fn.ret_type, desc.__isset.is_nullable ? desc.is_nullable : true)) {
    if (desc.agg_expr.__isset.param_types) {
        const auto& param_types = desc.agg_expr.param_types;
        for (const auto& param_type : param_types) {
            _argument_types_with_sort.push_back(
                    DataTypeFactory::instance().create_data_type(param_type));
        }
    }
}

Status AggFnEvaluator::create(ObjectPool* pool, const TExpr& desc, const TSortInfo& sort_info,
                              const bool without_key, const bool is_window_function,
                              AggFnEvaluator** result) {
    *result =
            pool->add(AggFnEvaluator::create_unique(desc.nodes[0], without_key, is_window_function)
                              .release());
    auto& agg_fn_evaluator = *result;
    int node_idx = 0;
    for (int i = 0; i < desc.nodes[0].num_children; ++i) {
        ++node_idx;
        VExprSPtr expr;
        VExprContextSPtr ctx;
        RETURN_IF_ERROR(VExpr::create_tree_from_thrift(desc.nodes, &node_idx, expr, ctx));
        agg_fn_evaluator->_input_exprs_ctxs.push_back(ctx);
    }

    auto sort_size = sort_info.ordering_exprs.size();
    auto real_arguments_size = agg_fn_evaluator->_argument_types_with_sort.size() - sort_size;
    // Child arguments contains [real arguments, order by arguments], we pass the arguments
    // to the order by functions
    for (int i = 0; i < sort_size; ++i) {
        agg_fn_evaluator->_sort_description.emplace_back(real_arguments_size + i,
                                                         sort_info.is_asc_order[i] ? 1 : -1,
                                                         sort_info.nulls_first[i] ? -1 : 1);
    }

    // Pass the real arguments to get functions
    for (int i = 0; i < real_arguments_size; ++i) {
        agg_fn_evaluator->_real_argument_types.emplace_back(
                agg_fn_evaluator->_argument_types_with_sort[i]);
    }
    return Status::OK();
}

Status AggFnEvaluator::prepare(RuntimeState* state, const RowDescriptor& desc,
                               const SlotDescriptor* intermediate_slot_desc,
                               const SlotDescriptor* output_slot_desc) {
    DCHECK(intermediate_slot_desc != nullptr);
    DCHECK(_intermediate_slot_desc == nullptr);
    _output_slot_desc = output_slot_desc;
    _intermediate_slot_desc = intermediate_slot_desc;

    Status status = VExpr::prepare(_input_exprs_ctxs, state, desc);
    RETURN_IF_ERROR(status);

    DataTypes tmp_argument_types;
    tmp_argument_types.reserve(_input_exprs_ctxs.size());

    std::vector<std::string_view> child_expr_name;

    // prepare for argument
    for (auto& _input_exprs_ctx : _input_exprs_ctxs) {
        auto data_type = _input_exprs_ctx->root()->data_type();
        tmp_argument_types.emplace_back(data_type);
        child_expr_name.emplace_back(_input_exprs_ctx->root()->expr_name());
    }

    std::vector<std::string> column_names;
    for (const auto& expr_ctx : _input_exprs_ctxs) {
        const auto& root = expr_ctx->root();
        if (!root->expr_name().empty() && !root->is_constant()) {
            column_names.emplace_back(root->expr_name());
        }
    }

    const DataTypes& argument_types =
            _real_argument_types.empty() ? tmp_argument_types : _real_argument_types;

    if (_fn.binary_type == TFunctionBinaryType::JAVA_UDF) {
        if (config::enable_java_support) {
            _function = AggregateJavaUdaf::create(_fn, argument_types, _data_type);
            RETURN_IF_ERROR(static_cast<AggregateJavaUdaf*>(_function.get())->check_udaf(_fn));
        } else {
            return Status::InternalError(
                    "Java UDAF is not enabled, you can change be config enable_java_support to "
                    "true and restart be.");
        }
    } else if (_fn.binary_type == TFunctionBinaryType::RPC) {
        _function = AggregateRpcUdaf::create(_fn, argument_types, _data_type);
    } else if (_fn.binary_type == TFunctionBinaryType::AGG_STATE) {
        if (argument_types.size() != 1) {
            return Status::InternalError("Agg state Function must input 1 argument but get {}",
                                         argument_types.size());
        }
        if (argument_types[0]->is_nullable()) {
            return Status::InternalError("Agg state function input type must be not nullable");
        }
        if (argument_types[0]->get_primitive_type() != PrimitiveType::TYPE_AGG_STATE) {
            return Status::InternalError(
                    "Agg state function input type must be agg_state but get {}",
                    argument_types[0]->get_family_name());
        }

        std::string type_function_name =
                assert_cast<const DataTypeAggState*>(argument_types[0].get())->get_function_name();
        if (type_function_name + AGG_UNION_SUFFIX == _fn.name.function_name) {
            if (_data_type->is_nullable()) {
                return Status::InternalError(
                        "Union function return type must be not nullable, real={}",
                        _data_type->get_name());
            }
            if (_data_type->get_primitive_type() != PrimitiveType::TYPE_AGG_STATE) {
                return Status::InternalError(
                        "Union function return type must be AGG_STATE, real={}",
                        _data_type->get_name());
            }
            _function = get_agg_state_function<AggregateStateUnion>(argument_types, _data_type);
        } else if (type_function_name + AGG_MERGE_SUFFIX == _fn.name.function_name) {
            auto type = assert_cast<const DataTypeAggState*>(argument_types[0].get())
                                ->get_nested_function()
                                ->get_return_type();
            if (!type->equals(*_data_type)) {
                return Status::InternalError("{}'s expect return type is {}, but input {}",
                                             argument_types[0]->get_name(), type->get_name(),
                                             _data_type->get_name());
            }
            _function = get_agg_state_function<AggregateStateMerge>(argument_types, _data_type);
        } else {
            return Status::InternalError("{} not match function {}", argument_types[0]->get_name(),
                                         _fn.name.function_name);
        }
    } else {
        // Here, only foreachv1 needs special treatment, and v2 can follow the normal code logic.
        if (AggregateFunctionSimpleFactory::is_foreach(_fn.name.function_name)) {
            _function = AggregateFunctionSimpleFactory::instance().get(
                    _fn.name.function_name, argument_types,
                    AggregateFunctionSimpleFactory::result_nullable_by_foreach(_data_type),
                    state->be_exec_version(),
                    {.enable_decimal256 = state->enable_decimal256(),
                     .is_window_function = _is_window_function,
                     .column_names = std::move(column_names)});
        } else {
            _function = AggregateFunctionSimpleFactory::instance().get(
                    _fn.name.function_name, argument_types, _data_type->is_nullable(),
                    state->be_exec_version(),
                    {.enable_decimal256 = state->enable_decimal256(),
                     .is_window_function = _is_window_function,
                     .column_names = std::move(column_names)});
        }
    }
    if (_function == nullptr) {
        return Status::InternalError("Agg Function {} is not implemented", _fn.signature);
    }

    if (!_sort_description.empty()) {
        _function = transform_to_sort_agg_function(_function, _argument_types_with_sort,
                                                   _sort_description, state);
    }

    // Foreachv2, like foreachv1, does not check the return type,
    // because its return type is related to the internal agg.
    if (!AggregateFunctionSimpleFactory::is_foreach(_fn.name.function_name) &&
        !AggregateFunctionSimpleFactory::is_foreachv2(_fn.name.function_name)) {
        if (state->be_exec_version() >= BE_VERSION_THAT_SUPPORT_NULLABLE_CHECK) {
            RETURN_IF_ERROR(
                    _function->verify_result_type(_without_key, argument_types, _data_type));
        }
    }
    _expr_name = fmt::format("{}({})", _fn.name.function_name, child_expr_name);
    return Status::OK();
}

Status AggFnEvaluator::open(RuntimeState* state) {
    return VExpr::open(_input_exprs_ctxs, state);
}

void AggFnEvaluator::create(AggregateDataPtr place) {
    _function->create(place);
}

void AggFnEvaluator::destroy(AggregateDataPtr place) {
    _function->destroy(place);
}

Status AggFnEvaluator::execute_single_add(Block* block, AggregateDataPtr place, Arena& arena) {
    RETURN_IF_ERROR(_calc_argument_columns(block));
    _function->add_batch_single_place(block->rows(), place, _agg_columns.data(), arena);
    return Status::OK();
}

Status AggFnEvaluator::execute_batch_add(Block* block, size_t offset, AggregateDataPtr* places,
                                         Arena& arena, bool agg_many) {
    RETURN_IF_ERROR(_calc_argument_columns(block));
    _function->add_batch(block->rows(), places, offset, _agg_columns.data(), arena, agg_many);
    return Status::OK();
}

Status AggFnEvaluator::execute_batch_add_selected(Block* block, size_t offset,
                                                  AggregateDataPtr* places, Arena& arena) {
    RETURN_IF_ERROR(_calc_argument_columns(block));
    _function->add_batch_selected(block->rows(), places, offset, _agg_columns.data(), arena);
    return Status::OK();
}

Status AggFnEvaluator::streaming_agg_serialize(Block* block, BufferWritable& buf,
                                               const size_t num_rows, Arena& arena) {
    RETURN_IF_ERROR(_calc_argument_columns(block));
    _function->streaming_agg_serialize(_agg_columns.data(), buf, num_rows, arena);
    return Status::OK();
}

Status AggFnEvaluator::streaming_agg_serialize_to_column(Block* block, MutableColumnPtr& dst,
                                                         const size_t num_rows, Arena& arena) {
    RETURN_IF_ERROR(_calc_argument_columns(block));
    _function->streaming_agg_serialize_to_column(_agg_columns.data(), dst, num_rows, arena);
    return Status::OK();
}

void AggFnEvaluator::insert_result_info(AggregateDataPtr place, IColumn* column) {
    _function->insert_result_into(place, *column);
}

void AggFnEvaluator::insert_result_info_vec(const std::vector<AggregateDataPtr>& places,
                                            size_t offset, IColumn* column, const size_t num_rows) {
    _function->insert_result_into_vec(places, offset, *column, num_rows);
}

void AggFnEvaluator::reset(AggregateDataPtr place) {
    _function->reset(place);
}

std::string AggFnEvaluator::debug_string(const std::vector<AggFnEvaluator*>& exprs) {
    std::stringstream out;
    out << "[";

    for (int i = 0; i < exprs.size(); ++i) {
        out << (i == 0 ? "" : " ") << exprs[i]->debug_string();
    }

    out << "]";
    return out.str();
}

std::string AggFnEvaluator::debug_string() const {
    std::stringstream out;
    out << "AggFnEvaluator(";
    out << _fn.signature;
    out << ")";
    return out.str();
}

Status AggFnEvaluator::_calc_argument_columns(Block* block) {
    SCOPED_TIMER(_expr_timer);
    _agg_columns.resize(_input_exprs_ctxs.size());
    std::vector<int> column_ids(_input_exprs_ctxs.size());
    for (int i = 0; i < _input_exprs_ctxs.size(); ++i) {
        int column_id = -1;
        RETURN_IF_ERROR(_input_exprs_ctxs[i]->execute(block, &column_id));
        column_ids[i] = column_id;
    }
    materialize_block_inplace(*block, column_ids.data(),
                              column_ids.data() + _input_exprs_ctxs.size());
    for (int i = 0; i < _input_exprs_ctxs.size(); ++i) {
        _agg_columns[i] = block->get_by_position(column_ids[i]).column.get();
    }
    return Status::OK();
}

AggFnEvaluator* AggFnEvaluator::clone(RuntimeState* state, ObjectPool* pool) {
    return pool->add(AggFnEvaluator::create_unique(*this, state).release());
}

AggFnEvaluator::AggFnEvaluator(AggFnEvaluator& evaluator, RuntimeState* state)
        : _fn(evaluator._fn),
          _is_merge(evaluator._is_merge),
          _without_key(evaluator._without_key),
          _is_window_function(evaluator._is_window_function),
          _argument_types_with_sort(evaluator._argument_types_with_sort),
          _real_argument_types(evaluator._real_argument_types),
          _intermediate_slot_desc(evaluator._intermediate_slot_desc),
          _output_slot_desc(evaluator._output_slot_desc),
          _sort_description(evaluator._sort_description),
          _data_type(evaluator._data_type),
          _function(evaluator._function),
          _expr_name(evaluator._expr_name),
          _agg_columns(evaluator._agg_columns) {
    if (evaluator._fn.binary_type == TFunctionBinaryType::JAVA_UDF) {
        DataTypes tmp_argument_types;
        tmp_argument_types.reserve(evaluator._input_exprs_ctxs.size());
        // prepare for argument
        for (auto& _input_exprs_ctx : evaluator._input_exprs_ctxs) {
            auto data_type = _input_exprs_ctx->root()->data_type();
            tmp_argument_types.emplace_back(data_type);
        }
        const DataTypes& argument_types =
                _real_argument_types.empty() ? tmp_argument_types : _real_argument_types;
        _function = AggregateJavaUdaf::create(evaluator._fn, argument_types, evaluator._data_type);
        THROW_IF_ERROR(static_cast<AggregateJavaUdaf*>(_function.get())->check_udaf(evaluator._fn));
    }
    DCHECK(_function != nullptr);

    _input_exprs_ctxs.resize(evaluator._input_exprs_ctxs.size());
    for (size_t i = 0; i < _input_exprs_ctxs.size(); i++) {
        WARN_IF_ERROR(evaluator._input_exprs_ctxs[i]->clone(state, _input_exprs_ctxs[i]), "");
    }
}

Status AggFnEvaluator::check_agg_fn_output(uint32_t key_size,
                                           const std::vector<vectorized::AggFnEvaluator*>& agg_fn,
                                           const RowDescriptor& output_row_desc) {
    auto name_and_types = VectorizedUtils::create_name_and_data_types(output_row_desc);
    for (uint32_t i = key_size, j = 0; i < name_and_types.size(); i++, j++) {
        auto&& [name, column_type] = name_and_types[i];
        auto agg_return_type = agg_fn[j]->function()->get_return_type();
        if (!column_type->equals(*agg_return_type)) {
            if (!column_type->is_nullable() || agg_return_type->is_nullable() ||
                !remove_nullable(column_type)->equals(*agg_return_type)) {
                return Status::InternalError(
                        "column_type not match data_types in agg node, column_type={}, "
                        "data_types={},column name={}",
                        column_type->get_name(), agg_return_type->get_name(), name);
            }
        }
    }
    return Status::OK();
}

#include "common/compile_check_end.h"
} // namespace doris::vectorized
