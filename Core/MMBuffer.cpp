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
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <stdexcept>

#ifdef MMKV_APPLE
#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif
#endif

using namespace std;

namespace mmkv {

MMBuffer::MMBuffer(size_t length) {
    if (length > SmallBufferSize()) {
        type = MMBufferType_Normal;
        isNoCopy = MMBufferCopy;
        size = length;
        ptr = malloc(size);
        if (!ptr) {
            throw std::runtime_error(strerror(errno));
        }
#ifdef MMKV_APPLE
        m_data = nil;
#endif
    } else {
        type = MMBufferType_Small;
        paddedSize = static_cast<uint8_t>(length);
    }
}

MMBuffer::MMBuffer(void *source, size_t length, MMBufferCopyFlag flag) : isNoCopy(flag) {
    if (isNoCopy == MMBufferCopy) {
        if (length > SmallBufferSize()) {
            type = MMBufferType_Normal;
            size = length;
            ptr = malloc(size);
            if (!ptr) {
                throw std::runtime_error(strerror(errno));
            }
            memcpy(ptr, source, size);
#ifdef MMKV_APPLE
            m_data = nil;
#endif
        } else {
            type = MMBufferType_Small;
            paddedSize = static_cast<uint8_t>(length);
            memcpy(paddedBuffer, source, length);
        }
    } else {
        type = MMBufferType_Normal;
        size = length;
        ptr = source;
#ifdef MMKV_APPLE
        m_data = nil;
#endif
    }
}

#ifdef MMKV_APPLE
MMBuffer::MMBuffer(NSData *data, MMBufferCopyFlag flag)
    : type(MMBufferType_Normal), ptr((void *) data.bytes), size(data.length), isNoCopy(flag) {
    if (isNoCopy == MMBufferCopy) {
        m_data = [data retain];
    } else {
        m_data = data;
    }
}
#endif

MMBuffer::MMBuffer(MMBuffer &&other) noexcept : type(other.type) {
    if (type == MMBufferType_Normal) {
        size = other.size;
        ptr = other.ptr;
        isNoCopy = other.isNoCopy;
#ifdef MMKV_APPLE
        m_data = other.m_data;
#endif
        other.detach();
    } else {
        paddedSize = other.paddedSize;
        memcpy(paddedBuffer, other.paddedBuffer, paddedSize);
    }
}

MMBuffer &MMBuffer::operator=(MMBuffer &&other) noexcept {
    if (type == MMBufferType_Normal) {
        if (other.type == MMBufferType_Normal) {
            std::swap(isNoCopy, other.isNoCopy);
            std::swap(size, other.size);
            std::swap(ptr, other.ptr);
#ifdef MMKV_APPLE
            std::swap(m_data, other.m_data);
#endif
        } else {
            type = MMBufferType_Small;
            if (isNoCopy == MMBufferCopy) {
#ifdef MMKV_APPLE
                if (m_data) {
                    [m_data release];
                } else if (ptr) {
                    free(ptr);
                }
#else
                if (ptr) {
                    free(ptr);
                }
#endif
            }
            paddedSize = other.paddedSize;
            memcpy(paddedBuffer, other.paddedBuffer, paddedSize);
        }
    } else {
        if (other.type == MMBufferType_Normal) {
            type = MMBufferType_Normal;
            isNoCopy = other.isNoCopy;
            size = other.size;
            ptr = other.ptr;
#ifdef MMKV_APPLE
            m_data = other.m_data;
#endif
            other.detach();
        } else {
            uint8_t tmp[SmallBufferSize()];
            memcpy(tmp, other.paddedBuffer, other.paddedSize);
            memcpy(other.paddedBuffer, paddedBuffer, paddedSize);
            memcpy(paddedBuffer, tmp, other.paddedSize);
            std::swap(paddedSize, other.paddedSize);
        }
    }

    return *this;
}

MMBuffer::~MMBuffer() {
    if (isStoredOnStack()) {
        return;
    }

#ifdef MMKV_APPLE
    if (m_data) {
        if (isNoCopy == MMBufferCopy) {
            [m_data release];
        }
        return;
    }
#endif

    if (isNoCopy == MMBufferCopy && ptr) {
        free(ptr);
    }
}

void MMBuffer::detach() {
    // type = MMBufferType_Small;
    // paddedSize = 0;
    auto memsetPtr = (size_t *) &type;
    *memsetPtr = 0;
}

} // namespace mmkv
