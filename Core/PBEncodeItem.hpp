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

#ifndef MMKV_PBENCODEITEM_HPP
#define MMKV_PBENCODEITEM_HPP

#include "MMKVPredef.h"

#include "MMBuffer.h"
#include <cstdint>
#include <memory.h>
#include <string>

namespace mmkv {

enum PBEncodeItemType {
    PBEncodeItemType_None,
    PBEncodeItemType_Data,
    PBEncodeItemType_Container,
#ifndef MMKV_IOS_OR_MAC
    PBEncodeItemType_String,
#else
    PBEncodeItemType_NSString,
    PBEncodeItemType_NSData,
    PBEncodeItemType_NSDate,
#endif
};

struct PBEncodeItem {
    PBEncodeItemType type;
    uint32_t compiledSize;
    uint32_t valueSize;
    union {
        const MMBuffer *bufferValue;
#ifndef MMKV_IOS_OR_MAC
        const std::string *strValue;
#else
        void *objectValue;
        void *tmpObjectValue; // this object should be released on dealloc
#endif
    } value;

    PBEncodeItem() : type(PBEncodeItemType_None), compiledSize(0), valueSize(0) { memset(&value, 0, sizeof(value)); }

#ifndef MMKV_IOS_OR_MAC

    // opt std::vector.push_back() on slow_path
    PBEncodeItem(PBEncodeItem &&other) = default;

#else

    PBEncodeItem(const PBEncodeItem &other)
        : type(other.type), compiledSize(other.compiledSize), valueSize(other.valueSize), value(other.value) {
        if (type == PBEncodeItemType_NSString) {
            if (value.tmpObjectValue != nullptr) {
                CFRetain(value.tmpObjectValue);
            }
        }
    }

    // opt std::vector.push_back() on slow_path
    PBEncodeItem(PBEncodeItem &&other)
        : type(other.type), compiledSize(other.compiledSize), valueSize(other.valueSize), value(other.value) {
        // omit unnecessary CFRetain() & CFRelease()
        other.type = PBEncodeItemType_None;
        //other.value = {};
    }

    // in fact this is never called in MMKV's usage, just provide it to meet std::vector's requirements
    PBEncodeItem &operator=(const PBEncodeItem &other) {
        if (this != &other) {
            type = other.type;
            compiledSize = other.compiledSize;
            valueSize = other.valueSize;
            value = other.value;

            if (type == PBEncodeItemType_NSString) {
                if (value.tmpObjectValue != nullptr) {
                    CFRetain(value.tmpObjectValue);
                }
            }
        }
        return *this;
    }

    ~PBEncodeItem() {
        if (type == PBEncodeItemType_NSString) {
            if (value.tmpObjectValue != nullptr) {
                CFRelease(value.tmpObjectValue);
                value.tmpObjectValue = nullptr;
            }
        }
    }

#endif // MMKV_IOS_OR_MAC
};

} // namespace mmkv

#endif //MMKV_PBENCODEITEM_HPP
