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

#include "vec/common/sort/sorter.h"

namespace doris::vectorized {
#include "common/compile_check_begin.h"

class HeapSorter final : public Sorter {
    ENABLE_FACTORY_CREATOR(HeapSorter);

public:
    HeapSorter(VSortExecExprs& vsort_exec_exprs, int64_t limit, int64_t offset, ObjectPool* pool,
               std::vector<bool>& is_asc_order, std::vector<bool>& nulls_first,
               const RowDescriptor& row_desc);

    ~HeapSorter() override = default;

    Status append_block(Block* block) override;

    Status prepare_for_read(bool is_spill) override;

    Status get_next(RuntimeState* state, Block* block, bool* eos) override;

    size_t data_size() const override;

    Field get_top_value() override;

private:
    Status _prepare_sort_descs(Block* block);

    size_t _data_size = 0;
    size_t _heap_size = 0;
    size_t _queue_row_num = 0;
    MergeSorterQueue _queue;
    std::unique_ptr<MergeSorterState> _state;
    IColumn::Permutation _reverse_buffer;
};

#include "common/compile_check_end.h"
} // namespace doris::vectorized
