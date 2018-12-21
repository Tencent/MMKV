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

#include "MMBuffer.h"
#include <cstdint>
#include <memory.h>
#include <string>

namespace mmkv {

enum PBEncodeItemType {
    PBEncodeItemType_None,
    PBEncodeItemType_String,
    PBEncodeItemType_Data,
    PBEncodeItemType_Container,
};

struct PBEncodeItem {
    PBEncodeItemType type;
    uint32_t compiledSize;
    uint32_t valueSize;
    union {
        const std::string *strValue;
        const MMBuffer *bufferValue;
    } value;

    PBEncodeItem() : type(PBEncodeItemType_None), compiledSize(0), valueSize(0) {
        memset(&value, 0, sizeof(value));
    }
};

} // namespace mmkv

#endif //MMKV_PBENCODEITEM_HPP
