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

#ifndef KeyValueHolder_hpp
#define KeyValueHolder_hpp

#include "MMBuffer.h"
#include "aes/AESCrypt.h"

namespace mmkv {

enum KeyValueHolderType : uint8_t {
    KeyValueHolderType_Direct, // store value directly
    KeyValueHolderType_Offset, // store value by offset
};

#pragma pack(push, 1)

struct KeyValueHolder {
    uint16_t computedKVSize;
    uint16_t keySize;
    uint32_t valueSize;
    uint32_t offset;

    KeyValueHolder(uint32_t keyLength, uint32_t valueLength, uint32_t offset);

    MMBuffer toMMBuffer(const void *basePtr) const;
};

// kv holder for encrypted mmkv
struct KeyValueHolderCrypt {
    KeyValueHolderType flag;
    uint8_t paddedValueSize;

    union {
        struct {
            uint16_t keySize;
            uint32_t valueSize;
            uint32_t offset;
            unsigned char aesVector[AES_KEY_LEN];
        };
#define KeyValueHolder_ValueSize (sizeof(keySize) + sizeof(valueSize) + sizeof(offset) + sizeof(aesVector))
        uint8_t value[KeyValueHolder_ValueSize];
    };

    KeyValueHolderCrypt(const void *valuePtr, size_t valueLength);

    KeyValueHolderCrypt(uint32_t keyLength, uint32_t valueLength, uint32_t offset, unsigned char *iv = nullptr);

    MMBuffer toMMBuffer(const void *basePtr) const;
};

#pragma pack(pop)

} // namespace mmkv

#endif /* KeyValueHolder_hpp */
