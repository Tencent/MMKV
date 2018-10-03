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

#import <cstdlib>

enum MMBufferCopyFlag : bool {
    MMBufferCopy = false,
    MMBufferNoCopy = true,
};

/*
* A smart buffer to avoid allocating too many small pieces of memory.
* Instrumentation tells that nano_malloc is not that effecient.
* PS: copying/moving is faster than NSMutableData's retain/release when size is small.
*/
class MMBuffer {
private:
    void *ptr;
    uint8_t smallBuffer[16];
    size_t size;
    MMBufferCopyFlag isNoCopy;

    static constexpr auto SmallBufferSize = sizeof(smallBuffer);

    inline bool canUseSmallBuffer() const { return (size <= SmallBufferSize); }

    inline bool iSUsingSmallBuffer() const { return (!ptr && size > 0 && canUseSmallBuffer()); }

public:
    MMBuffer(size_t length = 0);

    MMBuffer(void *source, size_t length, MMBufferCopyFlag noCopy = MMBufferCopy);

    MMBuffer(MMBuffer &&other) noexcept;

    void swap(MMBuffer &&other);

    ~MMBuffer();

    inline uint8_t *getPtr() const { return (uint8_t *) (ptr ? ptr : smallBuffer); }

    inline size_t length() const { return size; }

    bool operator==(const MMBuffer &other) const;

private:
    // just forbid it for possibly misuse
    MMBuffer(const MMBuffer &other) = delete;
    MMBuffer &operator=(const MMBuffer &other) = delete;
};

#endif //MMKV_MMBUFFER_H
