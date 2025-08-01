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
// https://github.com/ClickHouse/ClickHouse/blob/master/src/Common/formatIPv6.h
// and modified by Doris

#pragma once

#include <vec/common/hex.h>
#include <vec/common/string_utils/string_utils.h>
#include <vec/core/types.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <utility>

constexpr size_t IPV4_BINARY_LENGTH = 4;
constexpr size_t IPV4_MAX_TEXT_LENGTH = 15; /// Does not count tail zero byte.
constexpr size_t IPV6_MAX_TEXT_LENGTH = 39;
constexpr size_t IPV4_MIN_NUM_VALUE = 0;          //num value of '0.0.0.0'
constexpr size_t IPV4_MAX_NUM_VALUE = 4294967295; //num value of '255.255.255.255'
constexpr int IPV4_MAX_OCTET_VALUE = 255;         //max value of octet
constexpr size_t IPV4_OCTET_BITS = 8;
constexpr size_t DECIMAL_BASE = 10;
constexpr size_t IPV6_BINARY_LENGTH = 16;

namespace doris::vectorized {
#include "common/compile_check_begin.h"

extern const std::array<std::pair<const char*, size_t>, 256> one_byte_to_string_lookup_table;

/** Format 4-byte binary sequesnce as IPv4 text: 'aaa.bbb.ccc.ddd',
  * expects in out to be in BE-format, that is 0x7f000001 => "127.0.0.1".
  *
  * Any number of the tail bytes can be masked with given mask string.
  *
  * Assumptions:
  *     src is IPV4_BINARY_LENGTH long,
  *     dst is IPV4_MAX_TEXT_LENGTH long,
  *     mask_tail_octets <= IPV4_BINARY_LENGTH
  *     mask_string is NON-NULL, if mask_tail_octets > 0.
  *
  * Examples:
  *     format_ipv4(&0x7f000001, dst, mask_tail_octets = 0, nullptr);
  *         > dst == "127.0.0.1"
  *     format_ipv4(&0x7f000001, dst, mask_tail_octets = 1, "xxx");
  *         > dst == "127.0.0.xxx"
  *     format_ipv4(&0x7f000001, dst, mask_tail_octets = 1, "0");
  *         > dst == "127.0.0.0"
  */
inline void format_ipv4(const unsigned char* src, size_t src_size, char*& dst,
                        uint8_t mask_tail_octets = 0, const char* mask_string = "xxx") {
    const size_t mask_length = mask_string ? strlen(mask_string) : 0;
    const size_t limit = std::min(IPV4_BINARY_LENGTH, IPV4_BINARY_LENGTH - mask_tail_octets);
    const size_t padding = std::min(4 - src_size, limit);
    for (size_t octet = 0; octet < padding; ++octet) {
        *dst++ = '0';
        *dst++ = '.';
    }

    for (size_t octet = 4 - src_size; octet < limit; ++octet) {
        uint8_t value = 0;
        if constexpr (std::endian::native == std::endian::little)
            value = static_cast<uint8_t>(src[IPV4_BINARY_LENGTH - octet - 1]);
        else
            value = static_cast<uint8_t>(src[octet]);
        const uint8_t len = static_cast<uint8_t>(one_byte_to_string_lookup_table[value].second);
        const char* str = one_byte_to_string_lookup_table[value].first;

        memcpy(dst, str, len);
        dst += len;

        *dst++ = '.';
    }

    for (size_t mask = 0; mask < mask_tail_octets; ++mask) {
        memcpy(dst, mask_string, mask_length);
        dst += mask_length;

        *dst++ = '.';
    }

    dst--;
}

inline void format_ipv4(const unsigned char* src, char*& dst, uint8_t mask_tail_octets = 0,
                        const char* mask_string = "xxx") {
    format_ipv4(src, 4, dst, mask_tail_octets, mask_string);
}

/** Unsafe (no bounds-checking for src nor dst), optimized version of parsing IPv4 string.
 *
 * Parses the input string `src` and stores binary host-endian value into buffer pointed by `dst`,
 * which should be long enough.
 * That is "127.0.0.1" becomes 0x7f000001.
 *
 * In case of failure doesn't modify buffer pointed by `dst`.
 *
 * WARNING - this function is adapted to work with ReadBuffer, where src is the position reference (ReadBuffer::position())
 *           and eof is the ReadBuffer::eof() - therefore algorithm below does not rely on buffer's continuity.
 *           To parse strings use overloads below.
 *
 * @param src         - iterator (reference to pointer) over input string - warning - continuity is not guaranteed.
 * @param eof         - function returning true if iterator riched the end - warning - can break iterator's continuity.
 * @param dst         - where to put output bytes, expected to be non-null and at IPV4_BINARY_LENGTH-long.
 * @param first_octet - preparsed first octet
 * @return            - true if parsed successfully, false otherwise.
 */
template <typename T, typename EOFfunction>
    requires(std::is_same<typename std::remove_cv<T>::type, char>::value)
inline bool parse_ipv4(T*& src, EOFfunction eof, unsigned char* dst, int32_t first_octet = -1) {
    if (src == nullptr || first_octet > IPV4_MAX_OCTET_VALUE) {
        return false;
    }

    UInt32 result = 0;
    int offset = (IPV4_BINARY_LENGTH - 1) * IPV4_OCTET_BITS;
    if (first_octet >= 0) {
        result |= first_octet << offset;
        offset -= IPV4_OCTET_BITS;
    }

    for (; true; offset -= IPV4_OCTET_BITS, ++src) {
        if (eof()) {
            return false;
        }

        UInt32 value = 0;
        size_t len = 0;
        while (is_numeric_ascii(*src) && len <= 3) {
            value = value * DECIMAL_BASE + (*src - '0');
            ++len;
            ++src;
            if (eof()) {
                break;
            }
        }
        if (len == 0 || value > IPV4_MAX_OCTET_VALUE || (offset > 0 && (eof() || *src != '.'))) {
            return false;
        }
        result |= value << offset;

        if (offset == 0) {
            break;
        }
    }

    memcpy(dst, &result, sizeof(result));
    return true;
}

/// returns pointer to the right after parsed sequence or null on failed parsing
inline const char* parse_ipv4(const char* src, const char* end, unsigned char* dst) {
    if (parse_ipv4(
                src, [&src, end]() { return src == end; }, dst)) {
        return src;
    }
    return nullptr;
}

/// returns true if whole buffer was parsed successfully
inline bool parse_ipv4_whole(const char* src, const char* end, unsigned char* dst) {
    return parse_ipv4(src, end, dst) == end;
}

/// returns pointer to the right after parsed sequence or null on failed parsing
inline const char* parse_ipv4(const char* src, unsigned char* dst) {
    if (parse_ipv4(
                src, []() { return false; }, dst)) {
        return src;
    }
    return nullptr;
}

/// returns true if whole null-terminated string was parsed successfully
inline bool parse_ipv4_whole(const char* src, unsigned char* dst) {
    const char* end = parse_ipv4(src, dst);
    return end != nullptr && *end == '\0';
}

/// integer logarithm, return ceil(log(value, base)) (the smallest integer greater or equal than log(value, base)
inline constexpr UInt32 int_log(const UInt32 value, const UInt32 base, const bool carry) {
    return value >= base ? 1 + int_log(value / base, base, value % base || carry)
                         : value % base > 1 || carry;
}

/// Print integer in desired base, faster than sprintf.
/// NOTE This is not the best way. See https://github.com/miloyip/itoa-benchmark
/// But it doesn't matter here.
template <UInt32 base, typename T>
inline void print_integer(char*& out, T value) {
    if (value == 0) {
        *out++ = '0';
    } else {
        constexpr size_t buffer_size = sizeof(T) * int_log(256, base, false);

        char buf[buffer_size];
        auto ptr = buf;

        while (value > 0) {
            *ptr = hex_digit_lowercase(value % base);
            ++ptr;
            value /= base;
        }

        /// Copy to out reversed.
        while (ptr != buf) {
            --ptr;
            *out = *ptr;
            ++out;
        }
    }
}

/** Rewritten inet_ntop6 from http://svn.apache.org/repos/asf/apr/apr/trunk/network_io/unix/inet_pton.c
  * performs significantly faster than the reference implementation due to the absence of sprintf calls,
  * bounds checking, unnecessary string copying and length calculation.
  * @param src         - pointer to IPv6 (16 bytes) stored in little-endian byte order
  * @param dst         - where to put format result bytes
  * @param zeroed_tail_bytes_count - the parameter is currently not being used
  */
inline void format_ipv6(unsigned char* src, char*& dst, uint8_t zeroed_tail_bytes_count = 0) {
    struct {
        Int64 base, len;
    } best {-1, 0}, cur {-1, 0};
    std::array<UInt16, IPV6_BINARY_LENGTH / sizeof(UInt16)> words {};

    // the current function logic is processed in big endian manner
    // but ipv6 in doris is stored in little-endian byte order
    // so transfer to big-endian byte order first
    // compatible with parse_ipv6 function in format_ip.h
    std::reverse(src, src + IPV6_BINARY_LENGTH);

    /** Preprocess:
        *    Copy the input (bytewise) array into a wordwise array.
        *    Find the longest run of 0x00's in src[] for :: shorthanding. */
    for (size_t i = 0; i < (IPV6_BINARY_LENGTH - zeroed_tail_bytes_count); i += 2) {
        words[i / 2] = (uint16_t)(src[i] << 8) | src[i + 1];
    }

    for (size_t i = 0; i < words.size(); i++) {
        if (words[i] == 0) {
            if (cur.base == -1) {
                cur.base = i;
                cur.len = 1;
            } else {
                cur.len++;
            }
        } else {
            if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len) {
                    best = cur;
                }
                cur.base = -1;
            }
        }
    }

    if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len) {
            best = cur;
        }
    }
    if (best.base != -1 && best.len < 2) {
        best.base = -1;
    }

    /// Format the result.
    for (size_t i = 0; i < words.size(); i++) {
        /// Are we inside the best run of 0x00's?
        if (best.base != -1) {
            auto best_base = static_cast<size_t>(best.base);
            if (i >= best_base && i < (best_base + best.len)) {
                if (i == best_base) {
                    *dst++ = ':';
                }
                continue;
            }
        }
        /// Are we following an initial run of 0x00s or any real hex?
        if (i != 0) {
            *dst++ = ':';
        }
        /// Is this address an encapsulated IPv4?
        if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffffu))) {
            uint8_t ipv4_buffer[IPV4_BINARY_LENGTH] = {0};
            memcpy(ipv4_buffer, src + 12, IPV4_BINARY_LENGTH);
            // Due to historical reasons format_ipv4() takes ipv4 in BE format, but inside ipv6 we store it in LE-format.
            if constexpr (std::endian::native == std::endian::little) {
                std::reverse(std::begin(ipv4_buffer), std::end(ipv4_buffer));
            }
            format_ipv4(ipv4_buffer, dst,
                        std::min(zeroed_tail_bytes_count, static_cast<uint8_t>(IPV4_BINARY_LENGTH)),
                        "0");
            // format_ipv4 has already added a null-terminator for us.
            return;
        }
        print_integer<16>(dst, words[i]);
    }

    /// Was it a trailing run of 0x00's?
    if (best.base != -1 &&
        static_cast<size_t>(best.base) + static_cast<size_t>(best.len) == words.size()) {
        *dst++ = ':';
    }
}

/** Unsafe (no bounds-checking for src nor dst), optimized version of parsing IPv6 string.
*
* Parses the input string `src` and stores binary little-endian value into buffer pointed by `dst`,
* which should be long enough. In case of failure zeroes IPV6_BINARY_LENGTH bytes of buffer pointed by `dst`.
*
* WARNING - this function is adapted to work with ReadBuffer, where src is the position reference (ReadBuffer::position())
*           and eof is the ReadBuffer::eof() - therefore algorithm below does not rely on buffer's continuity.
*           To parse strings use overloads below.
*
* @param src         - iterator (reference to pointer) over input string - warning - continuity is not guaranteed.
* @param eof         - function returning true if iterator riched the end - warning - can break iterator's continuity.
* @param dst         - where to put output bytes in little-endian byte order, expected to be non-null and at IPV6_BINARY_LENGTH-long.
* @param first_block - preparsed first block
* @return            - true if parsed successfully, false otherwise.
*/
template <typename T, typename EOFfunction>
    requires(std::is_same<typename std::remove_cv<T>::type, char>::value)
inline bool parse_ipv6(T*& src, EOFfunction eof, unsigned char* dst, int32_t first_block = -1) {
    const auto clear_dst = [dst]() {
        std::memset(dst, '\0', IPV6_BINARY_LENGTH);
        return false;
    };

    if (src == nullptr || eof()) return clear_dst();

    int groups = 0;            /// number of parsed groups
    unsigned char* iter = dst; /// iterator over dst buffer
    unsigned char* zptr =
            nullptr; /// pointer into dst buffer array where all-zeroes block ("::") is started

    std::memset(dst, '\0', IPV6_BINARY_LENGTH);

    if (first_block >= 0) {
        *iter++ = static_cast<unsigned char>((first_block >> 8) & 0xffu);
        *iter++ = static_cast<unsigned char>(first_block & 0xffu);
        if (*src == ':') {
            zptr = iter;
            ++src;
        }
        ++groups;
    }

    bool group_start = true;

    while (!eof() && groups < 8) {
        if (*src == ':') {
            ++src;
            if (eof()) /// trailing colon is not allowed
                return clear_dst();

            group_start = true;

            if (*src == ':') {
                if (zptr != nullptr) /// multiple all-zeroes blocks are not allowed
                    return clear_dst();
                zptr = iter;
                ++src;
                if (!eof() && *src == ':') {
                    /// more than one all-zeroes block is not allowed
                    return clear_dst();
                }
                continue;
            }
            if (groups == 0) /// leading colon is not allowed
                return clear_dst();
        }

        /// mixed IPv4 parsing
        if (*src == '.') {
            if (groups <= 1 && zptr == nullptr) /// IPv4 block can't be the first
                return clear_dst();

            if (group_start) /// first octet of IPv4 should be already parsed as an IPv6 group
                return clear_dst();

            ++src;
            if (eof()) return clear_dst();

            /// last parsed group should be reinterpreted as a decimal value - it's the first octet of IPv4
            --groups;
            iter -= 2;

            UInt16 num = 0;
            for (int i = 0; i < 2; ++i) {
                unsigned char first = (iter[i] >> 4) & 0x0fu;
                unsigned char second = iter[i] & 0x0fu;
                if (first > 9 || second > 9) return clear_dst();
                (num *= 100) += first * 10 + second;
            }
            if (num > 255) return clear_dst();

            /// parse IPv4 with known first octet
            if (!parse_ipv4(src, eof, iter, num)) return clear_dst();

            if constexpr (std::endian::native == std::endian::little)
                std::reverse(iter, iter + IPV4_BINARY_LENGTH);

            iter += 4;
            groups += 2;
            break; /// IPv4 block is the last - end of parsing
        }

        if (!group_start) /// end of parsing
            break;
        group_start = false;

        UInt16 val = 0;  /// current decoded group
        int xdigits = 0; /// number of decoded hex digits in current group

        for (; !eof() && xdigits < 4; ++src, ++xdigits) {
            UInt8 num = unhex(*src);
            if (num == 0xFF) break;
            (val <<= 4) |= num;
        }

        if (xdigits == 0) /// end of parsing
            break;

        *iter++ = static_cast<unsigned char>((val >> 8) & 0xffu);
        *iter++ = static_cast<unsigned char>(val & 0xffu);
        ++groups;
    }

    /// either all 8 groups or all-zeroes block should be present
    if (groups < 8 && zptr == nullptr) return clear_dst();

    /// process all-zeroes block
    if (zptr != nullptr) {
        if (groups == 8) {
            /// all-zeroes block at least should be one
            /// 2001:0db8:86a3::08d3:1319:8a2e:0370:7344 not valid
            return clear_dst();
        }
        size_t msize = iter - zptr;
        std::memmove(dst + IPV6_BINARY_LENGTH - msize, zptr, msize);
        std::memset(zptr, '\0', IPV6_BINARY_LENGTH - (iter - dst));
    }

    /// the current function logic is processed in big endian manner
    /// but ipv6 in doris is stored in little-endian byte order
    /// so transfer to little-endian
    std::reverse(dst, dst + IPV6_BINARY_LENGTH);

    return true;
}

/// returns pointer to the right after parsed sequence or null on failed parsing
inline const char* parse_ipv6(const char* src, const char* end, unsigned char* dst) {
    if (parse_ipv6(
                src, [&src, end]() { return src == end; }, dst))
        return src;
    return nullptr;
}

/// returns true if whole buffer was parsed successfully
inline bool parse_ipv6_whole(const char* src, const char* end, unsigned char* dst) {
    return parse_ipv6(src, end, dst) == end;
}

/// returns pointer to the right after parsed sequence or null on failed parsing
inline const char* parse_ipv6(const char* src, unsigned char* dst) {
    if (parse_ipv6(
                src, []() { return false; }, dst))
        return src;
    return nullptr;
}

/// returns true if whole null-terminated string was parsed successfully
inline bool parse_ipv6_whole(const char* src, unsigned char* dst) {
    const char* end = parse_ipv6(src, dst);
    return end != nullptr && *end == '\0';
}

#include "common/compile_check_end.h"
} // namespace doris::vectorized
