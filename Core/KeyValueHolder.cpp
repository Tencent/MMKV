/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

#include "KeyValueHolder.h"
#include "PBUtility.h"

extern void foo() {
    mmkv::KeyValueHolderCrypt holder(nullptr, 0);
    NSLog(@"KeyValueHolder.aesVector:%zu", sizeof(holder.aesVector));
    NSLog(@"KeyValueHolder.value:%zu", sizeof(holder.value));

    NSLog(@"KeyValueHolder:%zu, KeyValueHolderCrypt:%zu, MMBuffer:%zu", sizeof(mmkv::KeyValueHolder),
          sizeof(mmkv::KeyValueHolderCrypt), sizeof(mmkv::MMBuffer));
}

namespace mmkv {

KeyValueHolder::KeyValueHolder(uint32_t keyLength, uint32_t valueLength, uint32_t off)
    : keySize(static_cast<uint16_t>(keyLength)), valueSize(valueLength), offset(off) {
    computedKVSize = static_cast<uint16_t>(pbRawVarint32Size(keySize) + keySize);
    computedKVSize += pbRawVarint32Size(valueLength);
}

MMBuffer KeyValueHolder::toMMBuffer(const void *basePtr) const {
    auto realPtr = (uint8_t *) basePtr + computedKVSize;
    return MMBuffer(realPtr, valueSize, MMBufferNoCopy);
}

KeyValueHolderCrypt::KeyValueHolderCrypt(const void *src, size_t length)
    : flag(KeyValueHolderType_Direct), paddedValueSize(static_cast<uint8_t>(length)) {
    assert(length <= KeyValueHolder_ValueSize);
    memcpy(value, src, length);
}

KeyValueHolderCrypt::KeyValueHolderCrypt(uint32_t keyLength, uint32_t valueLength, uint32_t off, unsigned char *iv)
    : flag(KeyValueHolderType_Offset)
    , paddedValueSize(0)
    , keySize(static_cast<uint16_t>(keyLength))
    , valueSize(valueLength)
    , offset(off) {
    if (iv) {
        memcpy(aesVector, iv, sizeof(aesVector));
    }
}

MMBuffer KeyValueHolderCrypt::toMMBuffer(const void *basePtr) const {
    if (flag == KeyValueHolderType_Direct) {
        return MMBuffer((void *) value, paddedValueSize, MMBufferNoCopy);
    } else {
        auto realPtr = (uint8_t *) basePtr + offset;
        // TODO: compact these two into one member variable
        realPtr += pbRawVarint32Size(keySize) + keySize;
        realPtr += pbRawVarint32Size(valueSize);
        return MMBuffer(realPtr, valueSize, MMBufferNoCopy);
    }
}

} // namespace mmkv
