/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MMKV_PBUTILITY_H
#define MMKV_PBUTILITY_H
#ifdef  __cplusplus

#include "MMKVPredef.h"

#include <cstdint>

#ifndef MMKV_WIN32
#    ifndef likely
#        define unlikely(x) (__builtin_expect(bool(x), 0))
#        define likely(x) (__builtin_expect(bool(x), 1))
#    endif
#else
#    ifndef likely
#        define unlikely(x) (x)
#        define likely(x) (x)
#    endif
#endif

namespace mmkv {

template <typename T, typename P>
union Converter {
    static_assert(sizeof(T) == sizeof(P), "size not match");
    T first;
    P second;
};

static inline int64_t Float64ToInt64(double v) {
    Converter<double, int64_t> converter;
    converter.first = v;
    return converter.second;
}

static inline int32_t Float32ToInt32(float v) {
    Converter<float, int32_t> converter;
    converter.first = v;
    return converter.second;
}

static inline double Int64ToFloat64(int64_t v) {
    Converter<double, int64_t> converter;
    converter.second = v;
    return converter.first;
}

static inline float Int32ToFloat32(int32_t v) {
    Converter<float, int32_t> converter;
    converter.second = v;
    return converter.first;
}

static inline uint64_t Int64ToUInt64(int64_t v) {
    Converter<int64_t, uint64_t> converter;
    converter.first = v;
    return converter.second;
}

static inline int64_t UInt64ToInt64(uint64_t v) {
    Converter<int64_t, uint64_t> converter;
    converter.second = v;
    return converter.first;
}

static inline uint32_t Int32ToUInt32(int32_t v) {
    Converter<int32_t, uint32_t> converter;
    converter.first = v;
    return converter.second;
}

static inline int32_t UInt32ToInt32(uint32_t v) {
    Converter<int32_t, uint32_t> converter;
    converter.second = v;
    return converter.first;
}

static inline int32_t logicalRightShift32(int32_t value, uint32_t spaces) {
    return UInt32ToInt32((Int32ToUInt32(value) >> spaces));
}

static inline int64_t logicalRightShift64(int64_t value, uint32_t spaces) {
    return UInt64ToInt64((Int64ToUInt64(value) >> spaces));
}

constexpr uint32_t LittleEdian32Size = 4;

constexpr uint32_t pbFloatSize() {
    return LittleEdian32Size;
}

constexpr uint32_t pbFixed32Size() {
    return LittleEdian32Size;
}

constexpr uint32_t LittleEdian64Size = 8;

constexpr uint32_t pbDoubleSize() {
    return LittleEdian64Size;
}

constexpr uint32_t pbBoolSize() {
    return 1;
}

extern uint32_t pbRawVarint32Size(uint32_t value);

static inline uint32_t pbRawVarint32Size(int32_t value) {
    return pbRawVarint32Size(Int32ToUInt32(value));
}

extern uint32_t pbUInt64Size(uint64_t value);

static inline uint32_t pbInt64Size(int64_t value) {
    return pbUInt64Size(Int64ToUInt64(value));
}

static inline uint32_t pbInt32Size(int32_t value) {
    if (value >= 0) {
        return pbRawVarint32Size(value);
    } else {
        return 10;
    }
}

static inline uint32_t pbUInt32Size(uint32_t value) {
    return pbRawVarint32Size(value);
}

static inline uint32_t pbMMBufferSize(const MMBuffer &data) {
    auto valueLength = static_cast<uint32_t>(data.length());
    return valueLength + pbUInt32Size(valueLength);
}

constexpr uint32_t Fixed32Size = pbFixed32Size();

} // namespace mmkv

#endif
#endif //MMKV_PBUTILITY_H
