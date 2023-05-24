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

#ifndef MMKV_MMBUFFER_H
#define MMKV_MMBUFFER_H
#ifdef __cplusplus

#include "MMKVPredef.h"

#include <cstdint>
#include <cstdlib>
#include <cstddef>

namespace mmkv {

enum MMBufferCopyFlag : bool {
    MMBufferCopy = false,
    MMBufferNoCopy = true,
};

#pragma pack(push, 1)

#ifndef MMKV_DISABLE_CRYPT
struct KeyValueHolderCrypt;
#endif

class MMBuffer {
    enum MMBufferType : uint8_t {
        MMBufferType_Small,  // store small buffer in stack memory
        MMBufferType_Normal, // store in heap memory
    };
    MMBufferType type;

    union {
        struct {
            MMBufferCopyFlag isNoCopy;
            size_t size;
            void *ptr;
#ifdef MMKV_APPLE
            NSData *m_data;
#endif
        };
        struct {
            uint8_t paddedSize;
            // make at least 10 bytes to hold all primitive types (negative int32, int64, double etc) on 32 bit device
            // on 64 bit device it's guaranteed larger than 10 bytes
            uint8_t paddedBuffer[10];
        };
    };

    static constexpr size_t SmallBufferSize() {
        return sizeof(MMBuffer) - offsetof(MMBuffer, paddedBuffer);
    }

public:
    explicit MMBuffer(size_t length = 0);
    MMBuffer(void *source, size_t length, MMBufferCopyFlag flag = MMBufferCopy);
#ifdef MMKV_APPLE
    explicit MMBuffer(NSData *data, MMBufferCopyFlag flag = MMBufferCopy);
#endif

    MMBuffer(MMBuffer &&other) noexcept;
    MMBuffer(MMBuffer &&other, size_t length) noexcept;
    MMBuffer &operator=(MMBuffer &&other) noexcept;

    ~MMBuffer();

    bool isStoredOnStack() const { return (type == MMBufferType_Small); }

    void *getPtr() const { return isStoredOnStack() ? (void *) paddedBuffer : ptr; }

    size_t length() const { return isStoredOnStack() ? paddedSize : size; }

    // transfer ownership to others
    void detach();

    // those are expensive, just forbid it for possibly misuse
    explicit MMBuffer(const MMBuffer &other) = delete;
    MMBuffer &operator=(const MMBuffer &other) = delete;

#ifndef MMKV_DISABLE_CRYPT
    friend KeyValueHolderCrypt;
#endif
};

#pragma pack(pop)

} // namespace mmkv

#endif
#endif //MMKV_MMBUFFER_H
