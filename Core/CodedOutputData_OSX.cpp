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

#include "CodedOutputData.h"

#ifdef MMKV_IOS_OR_MAC

#    include "PBUtility.h"
#    include <cstring>
#    include <stdexcept>

#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif

using namespace std;

namespace mmkv {

void CodedOutputData::writeString(__unsafe_unretained NSString *value) {
    NSUInteger numberOfBytes = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    this->writeRawVarint32((int32_t) numberOfBytes);
    if (m_position + numberOfBytes > m_size) {
        auto msg = "m_position: " + to_string(m_position) + ", numberOfBytes: " + to_string(numberOfBytes) +
                   ", m_size: " + to_string(m_size);
        throw out_of_range(msg);
    }

    [value getBytes:m_ptr + m_position
             maxLength:numberOfBytes
            usedLength:0
              encoding:NSUTF8StringEncoding
               options:0
                 range:NSMakeRange(0, value.length)
        remainingRange:nullptr];
    m_position += numberOfBytes;
}

} // namespace mmkv

#endif // MMKV_IOS_OR_MAC
