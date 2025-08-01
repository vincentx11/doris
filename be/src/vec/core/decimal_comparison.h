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
// https://github.com/ClickHouse/ClickHouse/blob/master/src/Core/DecimalComparison.h
// and modified by Doris

#pragma once

#include "vec/columns/column_const.h"
#include "vec/columns/column_vector.h"
#include "vec/common/arithmetic_overflow.h"
#include "vec/core/accurate_comparison.h"
#include "vec/core/block.h"
#include "vec/core/call_on_type_index.h"
#include "vec/core/types.h"
#include "vec/data_types/data_type_decimal.h"
#include "vec/functions/function_helpers.h" /// todo core should not depend on function"

namespace doris::vectorized {

inline bool allow_decimal_comparison(const DataTypePtr& left_type, const DataTypePtr& right_type) {
    if (is_decimal(left_type->get_primitive_type())) {
        if (is_decimal(right_type->get_primitive_type()) ||
            is_int_or_bool(right_type->get_primitive_type()))
            return true;
    } else if (is_int_or_bool(left_type->get_primitive_type()) &&
               is_decimal(right_type->get_primitive_type()))
        return true;
    return false;
}

template <size_t>
struct ConstructDecInt {
    static constexpr PrimitiveType Type = TYPE_INT;
};
template <>
struct ConstructDecInt<8> {
    static constexpr PrimitiveType Type = TYPE_BIGINT;
};
template <>
struct ConstructDecInt<16> {
    static constexpr PrimitiveType Type = TYPE_LARGEINT;
};
template <>
struct ConstructDecInt<32> {
    static constexpr PrimitiveType Type = TYPE_DECIMAL256;
};

template <PrimitiveType T, PrimitiveType U>
struct DecCompareInt {
    static constexpr PrimitiveType Type =
            ConstructDecInt < (!is_decimal(U) ||
                               sizeof(typename PrimitiveTypeTraits<T>::ColumnItemType) >
                                       sizeof(typename PrimitiveTypeTraits<U>::ColumnItemType))
                    ? sizeof(typename PrimitiveTypeTraits<T>::ColumnItemType)
                    : sizeof(typename PrimitiveTypeTraits<U>::ColumnItemType) > ::Type;
};

///
template <PrimitiveType A, PrimitiveType B,
          template <PrimitiveType, PrimitiveType> typename Operation, bool _check_overflow = true,
          bool _actual = is_decimal(A) || is_decimal(B)>
class DecimalComparison {
public:
    static constexpr PrimitiveType CompareIntPType = DecCompareInt<A, B>::Type;
    using CompareInt = typename PrimitiveTypeTraits<CompareIntPType>::CppNativeType;
    using Op = Operation<CompareIntPType, CompareIntPType>;
    using ColVecA = typename PrimitiveTypeTraits<A>::ColumnType;
    using ColVecB = typename PrimitiveTypeTraits<B>::ColumnType;
    using ArrayA = typename ColVecA::Container;
    using ArrayB = typename ColVecB::Container;

    DecimalComparison(Block& block, uint32_t result, const ColumnWithTypeAndName& col_left,
                      const ColumnWithTypeAndName& col_right) {
        if (!apply(block, result, col_left, col_right)) {
            throw Exception(Status::FatalError("Wrong decimal comparison with {} and {}",
                                               col_left.type->get_name(),
                                               col_right.type->get_name()));
        }
    }

    static bool apply(Block& block, uint32_t result [[maybe_unused]],
                      const ColumnWithTypeAndName& col_left,
                      const ColumnWithTypeAndName& col_right) {
        if constexpr (_actual) {
            ColumnPtr c_res;
            Shift shift = getScales<A, B>(col_left.type, col_right.type);

            c_res = apply_with_scale(col_left.column, col_right.column, shift);
            if (c_res) {
                block.replace_by_position(result, std::move(c_res));
            }
            return true;
        }
        return false;
    }

    static bool compare(typename PrimitiveTypeTraits<A>::ColumnItemType a,
                        typename PrimitiveTypeTraits<B>::ColumnItemType b, UInt32 scale_a,
                        UInt32 scale_b) {
        static const UInt32 max_scale = max_decimal_precision<TYPE_DECIMAL256>();
        if (scale_a > max_scale || scale_b > max_scale) {
            throw Exception(Status::FatalError("Bad scale of decimal field"));
        }

        Shift shift;
        if (scale_a < scale_b) {
            shift.a = typename PrimitiveTypeTraits<B>::DataType(max_decimal_precision<B>(), scale_b)
                              .get_scale_multiplier(scale_b - scale_a);
        }
        if (scale_a > scale_b) {
            shift.b = typename PrimitiveTypeTraits<A>::DataType(max_decimal_precision<A>(), scale_a)
                              .get_scale_multiplier(scale_a - scale_b);
        }

        return apply_with_scale(a, b, shift);
    }

private:
    struct Shift {
        CompareInt a = 1;
        CompareInt b = 1;

        bool none() const { return a == 1 && b == 1; }
        bool left() const { return a != 1; }
        bool right() const { return b != 1; }
    };

    template <typename T, typename U>
    static auto apply_with_scale(T a, U b, const Shift& shift) {
        if (shift.left())
            return apply<true, false>(a, b, shift.a);
        else if (shift.right())
            return apply<false, true>(a, b, shift.b);
        return apply<false, false>(a, b, 1);
    }

    template <PrimitiveType T, PrimitiveType U>
        requires(is_decimal(T) && is_decimal(U))
    static Shift getScales(const DataTypePtr& left_type, const DataTypePtr& right_type) {
        const typename PrimitiveTypeTraits<T>::DataType* decimal0 = check_decimal<T>(*left_type);
        const typename PrimitiveTypeTraits<U>::DataType* decimal1 = check_decimal<U>(*right_type);

        Shift shift;
        if (decimal0 && decimal1) {
            constexpr PrimitiveType Type =
                    sizeof(typename PrimitiveTypeTraits<T>::ColumnItemType) >=
                                    sizeof(typename PrimitiveTypeTraits<U>::ColumnItemType)
                            ? T
                            : U;
            auto type_ptr = decimal_result_type(*decimal0, *decimal1, false, false, false);
            const DataTypeDecimal<Type>* result_type = check_decimal<Type>(*type_ptr);
            shift.a = result_type->scale_factor_for(*decimal0);
            shift.b = result_type->scale_factor_for(*decimal1);
        } else if (decimal0) {
            shift.b = decimal0->get_scale_multiplier();
        } else if (decimal1) {
            shift.a = decimal1->get_scale_multiplier();
        }

        return shift;
    }

    template <PrimitiveType T, PrimitiveType U>
        requires(is_decimal(T) && !is_decimal(U))
    static Shift getScales(const DataTypePtr& left_type, const DataTypePtr&) {
        Shift shift;
        const typename PrimitiveTypeTraits<T>::DataTypeType* decimal0 =
                check_decimal<T>(*left_type);
        if (decimal0) {
            shift.b = decimal0->get_scale_multiplier();
        }
        return shift;
    }

    template <PrimitiveType T, PrimitiveType U>
        requires(!is_decimal(T) && is_decimal(U))
    static Shift getScales(const DataTypePtr&, const DataTypePtr& right_type) {
        Shift shift;
        const typename PrimitiveTypeTraits<U>::DataType* decimal1 = check_decimal<U>(*right_type);
        if (decimal1) {
            shift.a = decimal1->get_scale_multiplier();
        }
        return shift;
    }

    template <bool scale_left, bool scale_right>
    static ColumnPtr apply(const ColumnPtr& c0, const ColumnPtr& c1, CompareInt scale) {
        if constexpr (_actual) {
            bool c0_is_const = is_column_const(*c0);
            bool c1_is_const = is_column_const(*c1);

            if (c0_is_const && c1_is_const) {
                const ColumnConst* c0_const = check_and_get_column_const<ColVecA>(c0.get());
                const ColumnConst* c1_const = check_and_get_column_const<ColVecB>(c1.get());

                typename PrimitiveTypeTraits<A>::ColumnItemType a = c0_const->template get_value<
                        typename PrimitiveTypeTraits<A>::ColumnItemType>();
                typename PrimitiveTypeTraits<B>::ColumnItemType b = c1_const->template get_value<
                        typename PrimitiveTypeTraits<B>::ColumnItemType>();
                UInt8 res = apply<scale_left, scale_right>(a, b, scale);
                return DataTypeUInt8().create_column_const(c0->size(), to_field<TYPE_BOOLEAN>(res));
            }

            auto c_res = ColumnUInt8::create(c0->size());
            ColumnUInt8::Container& vec_res = c_res->get_data();

            if (c0_is_const) {
                const ColumnConst* c0_const = check_and_get_column_const<ColVecA>(c0.get());
                typename PrimitiveTypeTraits<A>::ColumnItemType a = c0_const->template get_value<
                        typename PrimitiveTypeTraits<A>::ColumnItemType>();
                if (const ColVecB* c1_vec = check_and_get_column<ColVecB>(c1.get()))
                    constant_vector<scale_left, scale_right>(a, c1_vec->get_data(), vec_res, scale);
                else {
                    throw Exception(Status::FatalError("Wrong column in Decimal comparison"));
                }
            } else if (c1_is_const) {
                const ColumnConst* c1_const = check_and_get_column_const<ColVecB>(c1.get());
                typename PrimitiveTypeTraits<B>::ColumnItemType b = c1_const->template get_value<
                        typename PrimitiveTypeTraits<B>::ColumnItemType>();
                if (const ColVecA* c0_vec = check_and_get_column<ColVecA>(c0.get()))
                    vector_constant<scale_left, scale_right>(c0_vec->get_data(), b, vec_res, scale);
                else {
                    throw Exception(Status::FatalError("Wrong column in Decimal comparison"));
                }
            } else {
                if (const ColVecA* c0_vec = check_and_get_column<ColVecA>(c0.get())) {
                    if (const ColVecB* c1_vec = check_and_get_column<ColVecB>(c1.get()))
                        vector_vector<scale_left, scale_right>(c0_vec->get_data(),
                                                               c1_vec->get_data(), vec_res, scale);
                    else {
                        throw Exception(Status::FatalError("Wrong column in Decimal comparison"));
                    }
                } else {
                    throw Exception(Status::FatalError("Wrong column in Decimal comparison"));
                }
            }
            return c_res;
        } else {
            return ColumnUInt8::create();
        }
    }

    template <bool scale_left, bool scale_right>
    static UInt8 apply(typename PrimitiveTypeTraits<A>::ColumnItemType a,
                       typename PrimitiveTypeTraits<B>::ColumnItemType b,
                       CompareInt scale [[maybe_unused]]) {
        CompareInt x = a;
        CompareInt y = b;

        if constexpr (_check_overflow) {
            bool overflow = false;

            if constexpr (sizeof(typename PrimitiveTypeTraits<A>::ColumnItemType) >
                          sizeof(CompareInt))
                overflow |= (typename PrimitiveTypeTraits<A>::ColumnItemType(x) != a);
            if constexpr (sizeof(typename PrimitiveTypeTraits<B>::ColumnItemType) >
                          sizeof(CompareInt))
                overflow |= (typename PrimitiveTypeTraits<B>::ColumnItemType(y) != b);
            if constexpr (IsUnsignedV<typename PrimitiveTypeTraits<A>::ColumnItemType>)
                overflow |= (x < 0);
            if constexpr (IsUnsignedV<typename PrimitiveTypeTraits<B>::ColumnItemType>)
                overflow |= (y < 0);

            if constexpr (scale_left) overflow |= common::mul_overflow(x, scale, x);
            if constexpr (scale_right) overflow |= common::mul_overflow(y, scale, y);

            if (overflow) {
                throw Exception(Status::FatalError("Can't compare"));
            }
        } else {
            if constexpr (scale_left) x *= scale;
            if constexpr (scale_right) y *= scale;
        }

        return Op::apply(x, y);
    }

    template <bool scale_left, bool scale_right>
    static void NO_INLINE vector_vector(const ArrayA& a, const ArrayB& b, PaddedPODArray<UInt8>& c,
                                        CompareInt scale) {
        size_t size = a.size();
        const typename PrimitiveTypeTraits<A>::ColumnItemType* a_pos = a.data();
        const typename PrimitiveTypeTraits<B>::ColumnItemType* b_pos = b.data();
        UInt8* c_pos = c.data();
        const typename PrimitiveTypeTraits<A>::ColumnItemType* a_end = a_pos + size;

        while (a_pos < a_end) {
            *c_pos = apply<scale_left, scale_right>(*a_pos, *b_pos, scale);
            ++a_pos;
            ++b_pos;
            ++c_pos;
        }
    }

    template <bool scale_left, bool scale_right>
    static void NO_INLINE vector_constant(const ArrayA& a,
                                          typename PrimitiveTypeTraits<B>::ColumnItemType b,
                                          PaddedPODArray<UInt8>& c, CompareInt scale) {
        size_t size = a.size();
        const typename PrimitiveTypeTraits<A>::ColumnItemType* a_pos = a.data();
        UInt8* c_pos = c.data();
        const typename PrimitiveTypeTraits<A>::ColumnItemType* a_end = a_pos + size;

        while (a_pos < a_end) {
            *c_pos = apply<scale_left, scale_right>(*a_pos, b, scale);
            ++a_pos;
            ++c_pos;
        }
    }

    template <bool scale_left, bool scale_right>
    static void NO_INLINE constant_vector(typename PrimitiveTypeTraits<A>::ColumnItemType a,
                                          const ArrayB& b, PaddedPODArray<UInt8>& c,
                                          CompareInt scale) {
        size_t size = b.size();
        const typename PrimitiveTypeTraits<B>::ColumnItemType* b_pos = b.data();
        UInt8* c_pos = c.data();
        const typename PrimitiveTypeTraits<B>::ColumnItemType* b_end = b_pos + size;

        while (b_pos < b_end) {
            *c_pos = apply<scale_left, scale_right>(a, *b_pos, scale);
            ++b_pos;
            ++c_pos;
        }
    }
};

} // namespace doris::vectorized
