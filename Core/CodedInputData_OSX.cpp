/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
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

#include "CodedInputData.h"

#ifdef MMKV_IOS_OR_MAC

#    include "PBUtility.h"
#    include <cassert>
#    include <stdexcept>

#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif

using namespace std;

namespace mmkv {

NSString *CodedInputData::readString() {
    int32_t size = this->readRawVarint32();
    if (size <= (m_size - m_position) && size > 0) {
        NSString *result =
            [[[NSString alloc] initWithBytes:(m_ptr + m_position) length:size encoding:NSUTF8StringEncoding]
                autorelease];
        m_position += size;
        return result;
    } else if (size == 0) {
        return @"";
    } else if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
    return nil;
}

NSData *CodedInputData::readNSData() {
    int32_t size = this->readRawVarint32();
    if (size <= m_size - m_position && size > 0) {
        NSData *result = [NSData dataWithBytes:(m_ptr + m_position) length:size];
        m_position += size;
        return result;
    } else if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
        return nil;
    }
    return nil;
}

} // namespace mmkv

#endif // MMKV_IOS_OR_MAC
