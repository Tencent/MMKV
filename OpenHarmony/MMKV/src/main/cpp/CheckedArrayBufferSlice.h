/*
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
 * Licensed under the BSD 3-Clause License.
 */

#pragma once

#include "napi/native_api.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace mmkv::ohos {

struct ArrayBufferSlice {
    uint8_t *data = nullptr;
    size_t length = 0;
};

inline bool NValueToSliceUInt32(napi_env env, napi_value value, uint32_t &result) {
    double number = 0;
    if (napi_get_value_double(env, value, &number) != napi_ok || !std::isfinite(number) || number < 0 ||
        number > std::numeric_limits<uint32_t>::max() || std::trunc(number) != number) {
        return false;
    }
    result = static_cast<uint32_t>(number);
    return true;
}

inline bool NValueToArrayBufferSlice(napi_env env, napi_value value, napi_value offsetValue, napi_value lengthValue,
                                   ArrayBufferSlice &result) {
    void *data = nullptr;
    size_t capacity = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    if (napi_get_arraybuffer_info(env, value, &data, &capacity) != napi_ok ||
        !NValueToSliceUInt32(env, offsetValue, offset) || !NValueToSliceUInt32(env, lengthValue, length) ||
        offset > capacity || length > capacity - offset || (length > 0 && !data)) {
        return false;
    }
    // An empty ArrayBuffer may have a null backing pointer. Empty slices need no pointer arithmetic.
    result.data = length == 0 ? nullptr : static_cast<uint8_t *>(data) + offset;
    result.length = length;
    return true;
}

template <typename T>
inline bool IsTypedArraySlice(const ArrayBufferSlice &slice) {
    return slice.length % sizeof(T) == 0 &&
           (slice.length == 0 || reinterpret_cast<uintptr_t>(slice.data) % alignof(T) == 0);
}

} // namespace mmkv::ohos
