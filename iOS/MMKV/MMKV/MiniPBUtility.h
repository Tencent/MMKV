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

#import <Foundation/Foundation.h>

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

static inline int32_t logicalRightShift32(int32_t value, int32_t spaces) {
	return UInt32ToInt32((Int32ToUInt32(value) >> spaces));
}

static inline int64_t logicalRightShift64(int64_t value, int32_t spaces) {
	return UInt64ToInt64((Int64ToUInt64(value) >> spaces));
}

constexpr uint32_t littleEdian32Size = 4;
constexpr uint32_t littleEdian64Size = 8;

constexpr int32_t pbDoubleSize(Float64 value) {
	return littleEdian64Size;
}

constexpr int32_t pbFloatSize(Float32 value) {
	return littleEdian32Size;
}

constexpr int32_t pbFixed32Size(int32_t value) {
	return littleEdian32Size;
}

constexpr int32_t pbBoolSize(BOOL value) {
	return 1;
}

extern int32_t pbUInt32Size(int32_t value);

extern int32_t pbUInt64Size(int64_t value);

extern int32_t pbInt64Size(int64_t value);

extern int32_t pbInt32Size(int32_t value);

extern int32_t pbRawVarint32Size(int32_t value);

extern int32_t pbRawVarint64Size(int64_t value);

extern int32_t pbDataSize(NSData *value);

extern int32_t pbRawStringSize(NSString *value);

extern int32_t pbStringSize(NSString *value);
