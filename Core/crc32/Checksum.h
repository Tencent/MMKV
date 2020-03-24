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

#pragma once
#include "../MMKVPredef.h"

#if MMKV_EMBED_ZLIB

#    include "zlib/zconf.h"

namespace zlib {

uLong crc32(uLong crc, const Bytef *buf, z_size_t len);

} // namespace zlib

#    define CRC32(crc, buf, len) zlib::crc32(crc, buf, len)

#else // MMKV_EMBED_ZLIB

#    ifdef __aarch64__

namespace mmkv {
uint32_t armv8_crc32(uint32_t crc, const unsigned char *buf, size_t len);
}

#        ifndef MMKV_ANDROID

#            define CRC32(crc, buf, len) mmkv::armv8_crc32(crc, buf, len)

#        else // Android runs on too many devices, have to check CPU's instruction set dynamically

typedef uint32_t (*CRC32_Func_t)(uint32_t crc, const unsigned char *buf, size_t len);
extern CRC32_Func_t CRC32;

#        endif // MMKV_ANDROID

#    else // __aarch64__

#        include <zlib.h>

#        define CRC32(crc, buf, len) ::crc32(crc, buf, static_cast<uInt>(len))

#    endif // __aarch64__

#endif // MMKV_EMBED_ZLIB
