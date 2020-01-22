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

#include "MMKVPredef.h"

#include <cstdint>
#include <cstdlib>

namespace mmkv {

enum MMBufferCopyFlag : bool {
    MMBufferCopy = false,
    MMBufferNoCopy = true,
};

class MMBuffer {
private:
    void *ptr;
    size_t size;
    MMBufferCopyFlag isNoCopy;
#ifdef MMKV_IOS_OR_MAC
    NSData *m_data = nil;
#endif

public:
    explicit MMBuffer(size_t length = 0);
    MMBuffer(void *source, size_t length, MMBufferCopyFlag flag = MMBufferCopy);
#ifdef MMKV_IOS_OR_MAC
    explicit MMBuffer(NSData *data, MMBufferCopyFlag flag = MMBufferCopy);
#endif

    MMBuffer(MMBuffer &&other) noexcept;
    MMBuffer &operator=(MMBuffer &&other) noexcept;

    ~MMBuffer();

    void *getPtr() const { return ptr; }

    size_t length() const { return size; }

    // those are expensive, just forbid it for possibly misuse
    explicit MMBuffer(const MMBuffer &other) = delete;
    MMBuffer &operator=(const MMBuffer &other) = delete;
};

} // namespace mmkv

#endif //MMKV_MMBUFFER_H
