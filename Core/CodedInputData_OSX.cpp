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

#ifdef MMKV_APPLE

#    include "PBUtility.h"
#    include <stdexcept>

#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif

using namespace std;

namespace mmkv {

NSString *CodedInputData::readString() {
    int32_t size = this->readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        auto ptr = m_ptr + m_position;
        NSString *result = [[NSString alloc] initWithBytes:ptr length:s_size encoding:NSUTF8StringEncoding];
        m_position += s_size;
        return [result autorelease];
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

NSString *CodedInputData::readString(KeyValueHolder &kvHolder) {
    kvHolder.offset = static_cast<uint32_t>(m_position);

    int32_t size = this->readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        kvHolder.keySize = static_cast<uint16_t>(s_size);

        auto ptr = m_ptr + m_position;
        NSString *result = [[NSString alloc] initWithBytes:ptr length:s_size encoding:NSUTF8StringEncoding];
        m_position += s_size;
        return [result autorelease];
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

NSData *CodedInputData::readNSData() {
    int32_t size = this->readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        NSData *result = [NSData dataWithBytes:(m_ptr + m_position) length:s_size];
        m_position += s_size;
        return result;
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

} // namespace mmkv

#endif // MMKV_APPLE
