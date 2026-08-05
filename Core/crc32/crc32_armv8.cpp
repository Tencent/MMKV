/*
* Tencent is pleased to support the open source community by making
* MMKV available.
*
* Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

#include "Checksum.h"

#include <algorithm>
#include <cstring>

#ifdef MMKV_USE_ARMV8_CRC32

#    ifdef CRC32
// nothing to do
#    elif MMKV_EMBED_ZLIB

static inline uint32_t _crc32Wrap(uint32_t crc, const uint8_t *buf, size_t len) {
    return static_cast<uint32_t>(zlib::crc32(crc, buf, static_cast<uInt>(len)));
}

CRC32_Func_t CRC32 = _crc32Wrap;

#    else
#        include <zlib.h>

static inline uint32_t _crc32Wrap(uint32_t crc, const uint8_t *buf, size_t len) {
    return static_cast<uint32_t>(::crc32(crc, buf, static_cast<uInt>(len)));
}

CRC32_Func_t CRC32 = _crc32Wrap;

#    endif

// targeting armv8 with crc instruction extension
#if defined(__GNUC__) && !defined(__clang__)
#    define TARGET_ARM_CRC __attribute__((target("+crc")))
#    define __builtin_arm_crc32b(a, b) __builtin_aarch64_crc32b(a, b)
#    define __builtin_arm_crc32h(a, b) __builtin_aarch64_crc32h(a, b)
#    define __builtin_arm_crc32w(a, b) __builtin_aarch64_crc32w(a, b)
#    define __builtin_arm_crc32d(a, b) __builtin_aarch64_crc32x(a, b)
#else
#    define TARGET_ARM_CRC __attribute__((target("crc")))
#endif

TARGET_ARM_CRC static inline uint32_t __crc32b(uint32_t a, uint8_t b) {
    return __builtin_arm_crc32b(a, b);
}

TARGET_ARM_CRC static inline uint32_t __crc32h(uint32_t a, uint16_t b) {
    return __builtin_arm_crc32h(a, b);
}

TARGET_ARM_CRC static inline uint32_t __crc32w(uint32_t a, uint32_t b) {
    return __builtin_arm_crc32w(a, b);
}

TARGET_ARM_CRC static inline uint32_t __crc32d(uint32_t a, uint64_t b) {
    return __builtin_arm_crc32d(a, b);
}

TARGET_ARM_CRC static inline uint32_t armv8_crc32_small(uint32_t crc, const uint8_t *buf, size_t len) {
    if (len >= sizeof(uint32_t)) {
        uint32_t word = 0;
        memcpy(&word, buf, sizeof(word));
        crc = __crc32w(crc, word);
        buf += sizeof(uint32_t);
        len -= sizeof(uint32_t);
    }
    if (len >= sizeof(uint16_t)) {
        uint16_t half = 0;
        memcpy(&half, buf, sizeof(half));
        crc = __crc32h(crc, half);
        buf += sizeof(uint16_t);
        len -= sizeof(uint16_t);
    }
    if (len >= sizeof(uint8_t)) {
        crc = __crc32b(crc, *(const uint8_t *) buf);
    }

    return crc;
}

namespace mmkv {

TARGET_ARM_CRC uint32_t armv8_crc32(uint32_t crc, const uint8_t *buf, size_t len) {

    crc = crc ^ 0xffffffffUL;

    // roundup to 8 byte pointer
    auto offset = std::min(len, (size_t) ((0U - (uintptr_t) buf) & 7U));
    if (offset) {
        crc = armv8_crc32_small(crc, buf, offset);
        buf += offset;
        len -= offset;
    }
    if (!len) {
        return crc ^ 0xffffffffUL;
    }

    // unroll to 8 * 8 byte per loop
    for (constexpr auto step = 8 * sizeof(uint64_t); len >= step; len -= step) {
        for (size_t index = 0; index < 8; index++) {
            uint64_t word = 0;
            memcpy(&word, buf, sizeof(word));
            crc = __crc32d(crc, word);
            buf += sizeof(word);
        }
    }

    for (constexpr auto step = sizeof(uint64_t); len >= step; len -= step) {
        uint64_t word = 0;
        memcpy(&word, buf, sizeof(word));
        crc = __crc32d(crc, word);
        buf += sizeof(word);
    }

    if (len) {
        crc = armv8_crc32_small(crc, buf, len);
    }

    return crc ^ 0xffffffffUL;
}

} // namespace mmkv

#endif // MMKV_USE_ARMV8_CRC32
