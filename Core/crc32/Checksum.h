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

#ifndef CHECKSUM_H
#define CHECKSUM_H
#ifdef __cplusplus

#include "../MMKVPredef.h"

#if MMKV_EMBED_ZLIB

#    include "zlib/zconf.h"

namespace zlib {

uLong crc32(uLong crc, const Bytef *buf, z_size_t len);

} // namespace zlib

#    define ZLIB_CRC32(crc, buf, len) zlib::crc32(crc, buf, len)

#else // MMKV_EMBED_ZLIB

#    include <zlib.h>

#    define ZLIB_CRC32(crc, buf, len) ::crc32(crc, buf, static_cast<uInt>(len))

#endif // MMKV_EMBED_ZLIB


#if defined(__aarch64__) && defined(__linux__)

#    define MMKV_USE_ARMV8_CRC32

namespace mmkv {
uint32_t armv8_crc32(uint32_t crc, const uint8_t *buf, size_t len);
}

#   ifdef MMKV_OHOS
// getauxval(AT_HWCAP) in OHOS returns wrong value, we just assume all OHOS device have crc32 instr
#       define CRC32 mmkv::armv8_crc32
#   else
// have to check CPU's instruction set dynamically
typedef uint32_t (*CRC32_Func_t)(uint32_t crc, const uint8_t *buf, size_t len);
extern CRC32_Func_t CRC32;
#   endif

#else // defined(__aarch64__) && defined(__linux__)

#    define CRC32(crc, buf, len) ZLIB_CRC32(crc, buf, len)

#endif // defined(__aarch64__) && defined(__linux__)

#endif // __cplusplus
#endif // CHECKSUM_H
