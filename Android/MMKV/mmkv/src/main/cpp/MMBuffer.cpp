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

#include "MMBuffer.h"
#include <cstdlib>
#include <cstring>
#include <utility>

MMBuffer::MMBuffer(size_t length) : ptr(nullptr), size(length), isNoCopy(MMBufferCopy) {
    if (size > 0) {
        ptr = malloc(size);
    }
}

MMBuffer::MMBuffer(void *source, size_t length, MMBufferCopyFlag noCopy)
    : ptr(source), size(length), isNoCopy(noCopy) {
    if (isNoCopy == MMBufferCopy) {
        ptr = malloc(size);
        memcpy(ptr, source, size);
    }
}

MMBuffer::MMBuffer(MMBuffer &&other) noexcept
    : ptr(other.ptr), size(other.size), isNoCopy(other.isNoCopy) {
    other.ptr = nullptr;
    other.size = 0;
    other.isNoCopy = MMBufferCopy;
}

MMBuffer &MMBuffer::operator=(MMBuffer &&other) noexcept {
    std::swap(ptr, other.ptr);
    std::swap(size, other.size);
    std::swap(isNoCopy, other.isNoCopy);

    return *this;
}

MMBuffer::~MMBuffer() {
    if (isNoCopy == MMBufferCopy && ptr) {
        free(ptr);
    }
    ptr = nullptr;
}
