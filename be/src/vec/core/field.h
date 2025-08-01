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
// https://github.com/ClickHouse/ClickHouse/blob/master/src/Core/Field.h
// and modified by Doris

#pragma once

#include <fmt/format.h>
#include <glog/logging.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/compiler_util.h" // IWYU pragma: keep
#include "common/exception.h"
#include "olap/hll.h"
#include "util/bitmap_value.h"
#include "util/quantile_state.h"
#include "vec/common/uint128.h"
#include "vec/core/types.h"
#include "vec/json/path_in_data.h"

namespace doris {
template <PrimitiveType type>
struct PrimitiveTypeTraits;
namespace vectorized {
template <typename T>
struct TypeName;
} // namespace vectorized
struct PackedInt128;
} // namespace doris

namespace doris::vectorized {

template <typename T>
struct NearestFieldTypeImpl {
    using Type = T; // for HLL or some origin types. see def. of storage
};

template <typename T>
using NearestFieldType = typename NearestFieldTypeImpl<T>::Type;

class Field;

using FieldVector = std::vector<Field>;

/// Array and Tuple use the same storage type -- FieldVector, but we declare
/// distinct types for them, so that the caller can choose whether it wants to
/// construct a Field of Array or a Tuple type. An alternative approach would be
/// to construct both of these types from FieldVector, and have the caller
/// specify the desired Field type explicitly.
struct Array : public FieldVector {
    using FieldVector::FieldVector;
};

struct Tuple : public FieldVector {
    using FieldVector::FieldVector;
};

struct Map : public FieldVector {
    using FieldVector::FieldVector;
};

struct FieldWithDataType;

using VariantMap = std::map<PathInData, Field>;

// Will replace VariantMap in the future
using VariantMapX = std::map<PathInData, FieldWithDataType>;

//TODO: rethink if we really need this? it only save one pointer from std::string
// not POD type so could only use read/write_json_binary instead of read/write_binary
class JsonbField {
public:
    JsonbField() = default;
    ~JsonbField() = default; // unique_ptr will handle cleanup automatically

    JsonbField(const char* ptr, size_t len) : size(len) {
        data = std::make_unique<char[]>(size);
        if (!data) {
            throw Exception(Status::FatalError("new data buffer failed, size: {}", size));
        }
        if (size > 0) {
            memcpy(data.get(), ptr, size);
        }
    }

    JsonbField(const JsonbField& x) : size(x.size) {
        data = std::make_unique<char[]>(size);
        if (!data) {
            throw Exception(Status::FatalError("new data buffer failed, size: {}", size));
        }
        if (size > 0) {
            memcpy(data.get(), x.data.get(), size);
        }
    }

    JsonbField(JsonbField&& x) noexcept : data(std::move(x.data)), size(x.size) { x.size = 0; }

    // dispatch for all type of storage. so need this. but not really used now.
    JsonbField& operator=(const JsonbField& x) {
        if (this != &x) {
            data = std::make_unique<char[]>(x.size);
            if (!data) {
                throw Exception(Status::FatalError("new data buffer failed, size: {}", x.size));
            }
            if (x.size > 0) {
                memcpy(data.get(), x.data.get(), x.size);
            }
            size = x.size;
        }
        return *this;
    }

    JsonbField& operator=(JsonbField&& x) noexcept {
        if (this != &x) {
            data = std::move(x.data);
            size = x.size;
            x.size = 0;
        }
        return *this;
    }

    const char* get_value() const { return data.get(); }
    size_t get_size() const { return size; }

    bool operator<(const JsonbField& r) const {
        throw Exception(Status::FatalError("comparing between JsonbField is not supported"));
    }
    bool operator<=(const JsonbField& r) const {
        throw Exception(Status::FatalError("comparing between JsonbField is not supported"));
    }
    bool operator==(const JsonbField& r) const {
        throw Exception(Status::FatalError("comparing between JsonbField is not supported"));
    }
    bool operator>(const JsonbField& r) const {
        throw Exception(Status::FatalError("comparing between JsonbField is not supported"));
    }
    bool operator>=(const JsonbField& r) const {
        throw Exception(Status::FatalError("comparing between JsonbField is not supported"));
    }
    bool operator!=(const JsonbField& r) const {
        throw Exception(Status::FatalError("comparing between JsonbField is not supported"));
    }

    const JsonbField& operator+=(const JsonbField& r) {
        throw Exception(Status::FatalError("Not support plus opration on JsonbField"));
    }

    const JsonbField& operator-=(const JsonbField& r) {
        throw Exception(Status::FatalError("Not support minus opration on JsonbField"));
    }

private:
    std::unique_ptr<char[]> data = nullptr;
    size_t size = 0;
};

template <typename T>
bool decimal_equal(T x, T y, UInt32 x_scale, UInt32 y_scale);
template <typename T>
bool decimal_less(T x, T y, UInt32 x_scale, UInt32 y_scale);
template <typename T>
bool decimal_less_or_equal(T x, T y, UInt32 x_scale, UInt32 y_scale);

template <typename T>
class DecimalField {
public:
    DecimalField(T value, UInt32 scale_) : dec(value), scale(scale_) {}
    // Store the underlying data ignoring scale.
    DecimalField(T value) : dec(value), scale(0) {}

    operator T() const { return dec; }
    T get_value() const { return dec; }
    UInt32 get_scale() const { return scale; }

    template <typename U>
    bool operator<(const DecimalField<U>& r) const {
        using MaxType = std::conditional_t<(sizeof(T) > sizeof(U)), T, U>;
        return decimal_less<MaxType>(dec, r.get_value(), scale, r.get_scale());
    }

    template <typename U>
    bool operator<=(const DecimalField<U>& r) const {
        using MaxType = std::conditional_t<(sizeof(T) > sizeof(U)), T, U>;
        return decimal_less_or_equal<MaxType>(dec, r.get_value(), scale, r.get_scale());
    }

    template <typename U>
    bool operator==(const DecimalField<U>& r) const {
        using MaxType = std::conditional_t<(sizeof(T) > sizeof(U)), T, U>;
        return decimal_equal<MaxType>(dec, r.get_value(), scale, r.get_scale());
    }

    template <typename U>
    bool operator>(const DecimalField<U>& r) const {
        return r < *this;
    }
    template <typename U>
    bool operator>=(const DecimalField<U>& r) const {
        return r <= *this;
    }
    template <typename U>
    bool operator!=(const DecimalField<U>& r) const {
        return !(*this == r);
    }

    const DecimalField<T>& operator+=(const DecimalField<T>& r) {
        if (scale != r.get_scale()) {
            throw Exception(Status::FatalError("Add different decimal fields"));
        }
        dec += r.get_value();
        return *this;
    }

    const DecimalField<T>& operator-=(const DecimalField<T>& r) {
        if (scale != r.get_scale()) {
            throw Exception(Status::FatalError("Sub different decimal fields"));
        }
        dec -= r.get_value();
        return *this;
    }

private:
    T dec;
    UInt32 scale;
};

/** 32 is enough. Round number is used for alignment and for better arithmetic inside std::vector.
  * NOTE: Actually, sizeof(std::string) is 32 when using libc++, so Field is 40 bytes.
  */
constexpr size_t DBMS_MIN_FIELD_SIZE = 32;

/** Discriminated union of several types.
  * Made for replacement of `boost::variant`
  *  is not generalized,
  *  but somewhat more efficient, and simpler.
  *
  * Used to represent a single value of one of several types in memory.
  * Warning! Prefer to use chunks of columns instead of single values. See Column.h
  */
class Field {
public:
    static const int MIN_NON_POD = 16;
    Field() : type(PrimitiveType::TYPE_NULL) {}
    // set Types::Null explictly and avoid other types
    Field(PrimitiveType w) : type(w) {}
    template <PrimitiveType T>
    static Field create_field(const typename PrimitiveTypeTraits<T>::NearestFieldType& data) {
        auto f = Field(PrimitiveTypeTraits<T>::NearestPrimitiveType);
        f.template create_concrete<PrimitiveTypeTraits<T>::NearestPrimitiveType>(data);
        return f;
    }
    template <PrimitiveType T>
    static Field create_field(const typename PrimitiveTypeTraits<T>::NearestFieldType&& data) {
        auto f = Field(PrimitiveTypeTraits<T>::NearestPrimitiveType);
        f.template create_concrete<PrimitiveTypeTraits<T>::NearestPrimitiveType>(data);
        return f;
    }

    /** Despite the presence of a template constructor, this constructor is still needed,
      *  since, in its absence, the compiler will still generate the default constructor.
      */
    Field(const Field& rhs) { create(rhs); }

    Field(Field&& rhs) { create(std::move(rhs)); }

    Field& operator=(const Field& rhs) {
        if (this != &rhs) {
            if (type != rhs.type) {
                destroy();
                create(rhs);
            } else {
                assign(rhs); /// This assigns string or vector without deallocation of existing buffer.
            }
        }
        return *this;
    }

    bool is_complex_field() const {
        return type == PrimitiveType::TYPE_ARRAY || type == PrimitiveType::TYPE_MAP ||
               type == PrimitiveType::TYPE_STRUCT || type == PrimitiveType::TYPE_VARIANT;
    }

    Field& operator=(Field&& rhs) {
        if (this != &rhs) {
            if (type != rhs.type) {
                destroy();
                create(std::move(rhs));
            } else {
                assign(std::move(rhs));
            }
        }
        return *this;
    }

    ~Field() { destroy(); }

    PrimitiveType get_type() const { return type; }
    std::string get_type_name() const;

    bool is_null() const { return type == PrimitiveType::TYPE_NULL; }

    // The template parameter T needs to be consistent with `which`.
    // If not, use NearestFieldType<> externally.
    // Maybe modify this in the future, reference: https://github.com/ClickHouse/ClickHouse/pull/22003
    template <typename T>
    T& get() {
        using TWithoutRef = std::remove_reference_t<T>;
        auto* MAY_ALIAS ptr = reinterpret_cast<TWithoutRef*>(&storage);
        return *ptr;
    }

    template <typename T>
    const T& get() const {
        using TWithoutRef = std::remove_reference_t<T>;
        const auto* MAY_ALIAS ptr = reinterpret_cast<const TWithoutRef*>(&storage);
        return *ptr;
    }

    bool operator==(const Field& rhs) const {
        return operator<=>(rhs) == std::strong_ordering::equal;
    }

    std::strong_ordering operator<=>(const Field& rhs) const {
        if (type == PrimitiveType::TYPE_NULL || rhs == PrimitiveType::TYPE_NULL) {
            return type <=> rhs.type;
        }
        if (type != rhs.type) {
            throw Exception(Status::FatalError("lhs type not equal with rhs, lhs={}, rhs={}",
                                               get_type_name(), rhs.get_type_name()));
        }

        switch (type) {
        case PrimitiveType::TYPE_BITMAP:
        case PrimitiveType::TYPE_HLL:
        case PrimitiveType::TYPE_QUANTILE_STATE:
        case PrimitiveType::INVALID_TYPE:
        case PrimitiveType::TYPE_JSONB:
        case PrimitiveType::TYPE_NULL:
        case PrimitiveType::TYPE_ARRAY:
        case PrimitiveType::TYPE_MAP:
        case PrimitiveType::TYPE_STRUCT:
        case PrimitiveType::TYPE_VARIANT:
            return std::strong_ordering::equal; //TODO: throw Exception?
        case PrimitiveType::TYPE_DATETIMEV2:
            return get<UInt64>() <=> rhs.get<UInt64>();
        case PrimitiveType::TYPE_DATEV2:
            return get<UInt32>() <=> rhs.get<UInt32>();
        case PrimitiveType::TYPE_DATE:
        case PrimitiveType::TYPE_DATETIME:
        case PrimitiveType::TYPE_BIGINT:
            return get<Int64>() <=> rhs.get<Int64>();
        case PrimitiveType::TYPE_LARGEINT:
            return get<Int128>() <=> rhs.get<Int128>();
        case PrimitiveType::TYPE_IPV6:
            return get<IPv6>() <=> rhs.get<IPv6>();
        case PrimitiveType::TYPE_IPV4:
            return get<IPv4>() <=> rhs.get<IPv4>();
        case PrimitiveType::TYPE_TIMEV2:
        case PrimitiveType::TYPE_DOUBLE:
            return get<Float64>() < rhs.get<Float64>()    ? std::strong_ordering::less
                   : get<Float64>() == rhs.get<Float64>() ? std::strong_ordering::equal
                                                          : std::strong_ordering::greater;
        case PrimitiveType::TYPE_STRING:
        case PrimitiveType::TYPE_CHAR:
        case PrimitiveType::TYPE_VARCHAR:
            return get<String>() <=> rhs.get<String>();
        case PrimitiveType::TYPE_DECIMAL32:
            return get<Decimal32>() <=> rhs.get<Decimal32>();
        case PrimitiveType::TYPE_DECIMAL64:
            return get<Decimal64>() <=> rhs.get<Decimal64>();
        case PrimitiveType::TYPE_DECIMALV2:
            return get<Decimal128V2>() <=> rhs.get<Decimal128V2>();
        case PrimitiveType::TYPE_DECIMAL128I:
            return get<Decimal128V3>() <=> rhs.get<Decimal128V3>();
        case PrimitiveType::TYPE_DECIMAL256:
            return get<Decimal256>() <=> rhs.get<Decimal256>();
        default:
            throw Exception(Status::FatalError("lhs type not equal with rhs, lhs={}, rhs={}",
                                               get_type_name(), rhs.get_type_name()));
        }
    }

    template <typename F,
              typename Field> /// Field template parameter may be const or non-const Field.
    static void dispatch(F&& f, Field& field) {
        switch (field.type) {
        case PrimitiveType::TYPE_NULL:
            f(field.template get<Null>());
            return;
        case PrimitiveType::TYPE_DATETIMEV2:
            f(field.template get<UInt64>());
            return;
        case PrimitiveType::TYPE_DATETIME:
        case PrimitiveType::TYPE_DATE:
        case PrimitiveType::TYPE_BIGINT:
            f(field.template get<Int64>());
            return;
        case PrimitiveType::TYPE_LARGEINT:
            f(field.template get<Int128>());
            return;
        case PrimitiveType::TYPE_IPV6:
            f(field.template get<IPv6>());
            return;
        case PrimitiveType::TYPE_TIMEV2:
        case PrimitiveType::TYPE_DOUBLE:
            f(field.template get<Float64>());
            return;
        case PrimitiveType::TYPE_STRING:
        case PrimitiveType::TYPE_CHAR:
        case PrimitiveType::TYPE_VARCHAR:
            f(field.template get<String>());
            return;
        case PrimitiveType::TYPE_JSONB:
            f(field.template get<JsonbField>());
            return;
        case PrimitiveType::TYPE_ARRAY:
            f(field.template get<Array>());
            return;
        case PrimitiveType::TYPE_STRUCT:
            f(field.template get<Tuple>());
            return;
        case PrimitiveType::TYPE_MAP:
            f(field.template get<Map>());
            return;
        case PrimitiveType::TYPE_DECIMAL32:
            f(field.template get<DecimalField<Decimal32>>());
            return;
        case PrimitiveType::TYPE_DECIMAL64:
            f(field.template get<DecimalField<Decimal64>>());
            return;
        case PrimitiveType::TYPE_DECIMALV2:
            f(field.template get<DecimalField<Decimal128V2>>());
            return;
        case PrimitiveType::TYPE_DECIMAL128I:
            f(field.template get<DecimalField<Decimal128V3>>());
            return;
        case PrimitiveType::TYPE_DECIMAL256:
            f(field.template get<DecimalField<Decimal256>>());
            return;
        case PrimitiveType::TYPE_VARIANT:
            f(field.template get<VariantMap>());
            return;
        case PrimitiveType::TYPE_BITMAP:
            f(field.template get<BitmapValue>());
            return;
        case PrimitiveType::TYPE_HLL:
            f(field.template get<HyperLogLog>());
            return;
        case PrimitiveType::TYPE_QUANTILE_STATE:
            f(field.template get<QuantileState>());
            return;
        default:
            throw Exception(
                    Status::FatalError("type not supported, type={}", field.get_type_name()));
        }
    }

    std::string_view as_string_view() const;

private:
    std::aligned_union_t<DBMS_MIN_FIELD_SIZE - sizeof(PrimitiveType), Null, UInt64, UInt128, Int64,
                         Int128, IPv6, Float64, String, JsonbField, Array, Tuple, Map, VariantMap,
                         DecimalField<Decimal32>, DecimalField<Decimal64>,
                         DecimalField<Decimal128V2>, DecimalField<Decimal128V3>,
                         DecimalField<Decimal256>, BitmapValue, HyperLogLog, QuantileState>
            storage;

    PrimitiveType type;

    /// Assuming there was no allocated state or it was deallocated (see destroy).
    template <PrimitiveType Type>
    void create_concrete(typename PrimitiveTypeTraits<Type>::NearestFieldType&& x);
    template <PrimitiveType Type>
    void create_concrete(const typename PrimitiveTypeTraits<Type>::NearestFieldType& x);
    /// Assuming same types.
    template <PrimitiveType Type>
    void assign_concrete(typename PrimitiveTypeTraits<Type>::NearestFieldType&& x);
    template <PrimitiveType Type>
    void assign_concrete(const typename PrimitiveTypeTraits<Type>::NearestFieldType& x);

    void create(const Field& field);

    void assign(const Field& x);

    void destroy();

    template <typename T>
    void destroy() {
        T* MAY_ALIAS ptr = reinterpret_cast<T*>(&storage);
        ptr->~T();
    }
};

struct FieldWithDataType {
    Field field;
    // used for nested type of array
    PrimitiveType base_scalar_type_id = PrimitiveType::INVALID_TYPE;
    uint8_t num_dimensions = 0;
    int precision = -1;
    int scale = -1;
};

template <typename T>
T get(const Field& field) {
    return field.template get<T>();
}

template <typename T>
T get(Field& field) {
    return field.template get<T>();
}

/// char may be signed or unsigned, and behave identically to signed char or unsigned char,
///  but they are always three different types.
/// signedness of char is different in Linux on x86 and Linux on ARM.
template <>
struct NearestFieldTypeImpl<char> {
    using Type = std::conditional_t<IsSignedV<char>, Int64, UInt64>;
};
template <>
struct NearestFieldTypeImpl<signed char> {
    using Type = Int64;
};
template <>
struct NearestFieldTypeImpl<unsigned char> {
    using Type = Int64;
};

template <>
struct NearestFieldTypeImpl<UInt16> {
    using Type = UInt64;
};
template <>
struct NearestFieldTypeImpl<UInt32> {
    using Type = UInt64;
};

template <>
struct NearestFieldTypeImpl<Int16> {
    using Type = Int64;
};
template <>
struct NearestFieldTypeImpl<Int32> {
    using Type = Int64;
};

/// long and long long are always different types that may behave identically or not.
/// This is different on Linux and Mac.
template <>
struct NearestFieldTypeImpl<long> {
    using Type = Int64;
};

template <>
struct NearestFieldTypeImpl<Decimal32> {
    using Type = DecimalField<Decimal32>;
};
template <>
struct NearestFieldTypeImpl<Decimal64> {
    using Type = DecimalField<Decimal64>;
};
template <>
struct NearestFieldTypeImpl<Decimal128V2> {
    using Type = DecimalField<Decimal128V2>;
};
template <>
struct NearestFieldTypeImpl<Decimal128V3> {
    using Type = DecimalField<Decimal128V3>;
};
template <>
struct NearestFieldTypeImpl<Decimal256> {
    using Type = DecimalField<Decimal256>;
};
template <>
struct NearestFieldTypeImpl<DecimalField<Decimal32>> {
    using Type = DecimalField<Decimal32>;
};
template <>
struct NearestFieldTypeImpl<DecimalField<Decimal64>> {
    using Type = DecimalField<Decimal64>;
};
template <>
struct NearestFieldTypeImpl<DecimalField<Decimal128V2>> {
    using Type = DecimalField<Decimal128V2>;
};
template <>
struct NearestFieldTypeImpl<DecimalField<Decimal128V3>> {
    using Type = DecimalField<Decimal128V3>;
};
template <>
struct NearestFieldTypeImpl<DecimalField<Decimal256>> {
    using Type = DecimalField<Decimal256>;
};
template <>
struct NearestFieldTypeImpl<Float32> {
    using Type = Float64;
};
template <>
struct NearestFieldTypeImpl<const char*> {
    using Type = String;
};
template <>
struct NearestFieldTypeImpl<bool> {
    using Type = UInt64;
};

template <>
struct NearestFieldTypeImpl<std::string_view> {
    using Type = String;
};

template <>
struct NearestFieldTypeImpl<PackedInt128> {
    using Type = Int128;
};

template <typename T>
decltype(auto) cast_to_nearest_field_type(T&& x) {
    using U = NearestFieldType<std::decay_t<T>>;
    if constexpr (std::is_same_v<PackedInt128, std::decay_t<T>>) {
        return U(x.value);
    } else if constexpr (std::is_same_v<std::decay_t<T>, U>) {
        return std::forward<T>(x);
    } else {
        return U(x);
    }
}

} // namespace doris::vectorized

template <>
struct std::hash<doris::vectorized::Field> {
    size_t operator()(const doris::vectorized::Field& field) const {
        if (field.is_null()) {
            return 0;
        }
        std::hash<std::string_view> hasher;
        return hasher(field.as_string_view());
    }
};
