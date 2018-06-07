//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_PBUTILITY_H
#define MMKV_PBUTILITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int64_t convertFloat64ToInt64(double v) {
    union { double f; int64_t i; } u;
    u.f = v;
    return u.i;
}


static inline int32_t convertFloat32ToInt32(float v) {
    union { float f; int32_t i; } u;
    u.f = v;
    return u.i;
}


static inline double convertInt64ToFloat64(int64_t v) {
    union { double f; int64_t i; } u;
    u.i = v;
    return u.f;
}


static inline float convertInt32ToFloat32(int32_t v) {
    union { float f; int32_t i; } u;
    u.i = v;
    return u.f;
}


static inline uint64_t convertInt64ToUInt64(int64_t v) {
    union { int64_t i; uint64_t u; } u;
    u.i = v;
    return u.u;
}


static inline int64_t convertUInt64ToInt64(uint64_t v) {
    union { int64_t i; uint64_t u; } u;
    u.u = v;
    return u.i;
}

static inline uint32_t convertInt32ToUInt32(int32_t v) {
    union { int32_t i; uint32_t u; } u;
    u.i = v;
    return u.u;
}

static inline int64_t convertUInt32ToInt32(uint32_t v) {
    union { int32_t i; uint32_t u; } u;
    u.u = v;
    return u.i;
}

static inline int32_t logicalRightShift32(int32_t value, int32_t spaces) {
    return (int32_t)convertUInt32ToInt32((convertInt32ToUInt32(value) >> spaces));
}

static inline int64_t logicalRightShift64(int64_t value, int32_t spaces) {
    return convertUInt64ToInt64((convertInt64ToUInt64(value) >> spaces));
}

#define LITTLE_ENDIAN_32_SIZE 4
#define LITTLE_ENDIAN_64_SIZE 8

constexpr uint32_t computeDoubleSize(double value) {
    return LITTLE_ENDIAN_64_SIZE;
}

constexpr uint32_t computeFloatSize(float value) {
    return LITTLE_ENDIAN_32_SIZE;
}

uint32_t computeInt64Size(int64_t value);
uint32_t computeInt32Size(int32_t value);

constexpr uint32_t computeFixed32Size(int32_t value) {
    return LITTLE_ENDIAN_32_SIZE;
}

uint32_t computeBoolSize(bool value);
// TODO: string & buffer support
//int32_t computeStringSize(NSString* value);
//int32_t computeDataSize(NSData* value);
uint32_t computeUInt32Size(int32_t value);

/**
 * Compute the number of bytes that would be needed to encode a varint.
 * {@code value} is treated as unsigned, so it won't be sign-extended if
 * negative.
 */
uint32_t computeRawVarint32Size(int32_t value);
uint32_t computeRawVarint64Size(int64_t value);

// TODO: string & buffer support
//int32_t computeRawStringSize(NSString* value);

#ifdef __cplusplus
}
#endif

#endif //MMKV_PBUTILITY_H
