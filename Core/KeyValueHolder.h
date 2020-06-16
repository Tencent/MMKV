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
#ifdef  __cplusplus

#include "MMBuffer.h"

namespace mmkv {

#pragma pack(push, 1)

struct KeyValueHolder {
    uint16_t computedKVSize; // internal use only
    uint16_t keySize;
    uint32_t valueSize;
    uint32_t offset;

    KeyValueHolder() = default;
    KeyValueHolder(uint32_t keyLength, uint32_t valueLength, uint32_t offset);

    static uint16_t computKVSize(uint32_t keySize, uint32_t valueSize);

    MMBuffer toMMBuffer(const void *basePtr) const;

    inline size_t end() const { return offset + computedKVSize + valueSize; }

    inline bool operator<(const KeyValueHolder &other) const { return (offset < other.offset); }
};

enum KeyValueHolderType : uint8_t {
    KeyValueHolderType_Direct, // store value directly
    KeyValueHolderType_Memory, // store value in the heap memory
    KeyValueHolderType_Offset, // store value by offset
};

class AESCrypt;
class AESCryptStatus;

// kv holder for encrypted mmkv
struct KeyValueHolderCrypt {
    KeyValueHolderType type;

    union {
        // store value by offset
        struct {
            uint8_t pbKeyValueSize; // size needed to encode keySize & valueSize
            uint16_t keySize;
            uint32_t valueSize;
            uint32_t offset;
            uint8_t aesNumber;
            uint8_t aesVector[AES_KEY_LEN];
        };
        // store value directly
        struct {
            uint8_t paddedSize;
            uint8_t value[0];
        };
        // store value in the heap memory
        struct {
            uint32_t memSize;
            void *memPtr;
        };
    };

    static constexpr size_t SmallBufferSize() {
        return sizeof(KeyValueHolderCrypt) - offsetof(KeyValueHolderCrypt, value);
    }

    KeyValueHolderCrypt() = default;
    KeyValueHolderCrypt(const void *valuePtr, size_t valueLength);
    KeyValueHolderCrypt(uint32_t keyLength, uint32_t valueLength, uint32_t offset);

    KeyValueHolderCrypt(KeyValueHolderCrypt &&other) noexcept;
    KeyValueHolderCrypt &operator=(KeyValueHolderCrypt &&other) noexcept;

    ~KeyValueHolderCrypt();

    AESCryptStatus *cryptStatus();

    MMBuffer toMMBuffer(const void *basePtr, const AESCrypt *crypter = nullptr) const;

    size_t end() const;

    bool operator<(const KeyValueHolderCrypt &other) const;

    // those are expensive, just forbid it for possibly misuse
    explicit KeyValueHolderCrypt(const KeyValueHolderCrypt &other) = delete;
    KeyValueHolderCrypt &operator=(const KeyValueHolderCrypt &other) = delete;

#ifndef NDEBUG
        static void testAESToMMBuffer();
#endif
};

#pragma pack(pop)

} // namespace mmkv

#endif
#endif /* KeyValueHolder_hpp */
