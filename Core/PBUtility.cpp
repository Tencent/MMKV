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

#include "PBUtility.h"

namespace mmkv {

uint32_t pbRawVarint32Size(uint32_t value) {
    if ((value & (0xffffffff << 7)) == 0) {
        return 1;
    } else if ((value & (0xffffffff << 14)) == 0) {
        return 2;
    } else if ((value & (0xffffffff << 21)) == 0) {
        return 3;
    } else if ((value & (0xffffffff << 28)) == 0) {
        return 4;
    }
    return 5;
}

uint32_t pbUInt64Size(uint64_t value) {
    if ((value & (0xffffffffffffffffL << 7)) == 0) {
        return 1;
    } else if ((value & (0xffffffffffffffffL << 14)) == 0) {
        return 2;
    } else if ((value & (0xffffffffffffffffL << 21)) == 0) {
        return 3;
    } else if ((value & (0xffffffffffffffffL << 28)) == 0) {
        return 4;
    } else if ((value & (0xffffffffffffffffL << 35)) == 0) {
        return 5;
    } else if ((value & (0xffffffffffffffffL << 42)) == 0) {
        return 6;
    } else if ((value & (0xffffffffffffffffL << 49)) == 0) {
        return 7;
    } else if ((value & (0xffffffffffffffffL << 56)) == 0) {
        return 8;
    } else if ((value & (0xffffffffffffffffL << 63)) == 0) {
        return 9;
    }
    return 10;
}

} // namespace mmkv
