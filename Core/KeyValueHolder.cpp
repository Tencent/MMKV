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
#include "aes/AESCrypt.h"

namespace mmkv {

KeyValueHolder::KeyValueHolder(uint32_t keyLength, uint32_t valueLength, uint32_t off)
    : keySize(static_cast<uint16_t>(keyLength)), valueSize(valueLength), offset(off) {
    computedKVSize = computKVSize(keyLength, valueLength);
}

uint16_t KeyValueHolder::computKVSize(uint32_t keySize, uint32_t valueSize) {
    uint16_t computedKVSize = static_cast<uint16_t>(keySize + pbRawVarint32Size(keySize));
    computedKVSize += static_cast<uint16_t>(pbRawVarint32Size(valueSize));
    return computedKVSize;
}

MMBuffer KeyValueHolder::toMMBuffer(const void *basePtr) const {
    auto realPtr = (uint8_t *) basePtr + offset;
    realPtr += computedKVSize;
    return MMBuffer(realPtr, valueSize, MMBufferNoCopy);
}

KeyValueHolderCrypt::KeyValueHolderCrypt(const void *src, size_t length)
    : flag(KeyValueHolderType_Direct), paddedSize(static_cast<uint8_t>(length)) {
    assert(length <= SmallBufferSize());
    memcpy(value, src, length);
}

KeyValueHolderCrypt::KeyValueHolderCrypt(uint32_t keyLength, uint32_t valueLength, uint32_t off, unsigned char *iv)
    : flag(KeyValueHolderType_Offset), keySize(static_cast<uint16_t>(keyLength)), valueSize(valueLength), offset(off) {
    pbKeyValueSize = static_cast<uint8_t>(pbRawVarint32Size(keySize) + pbRawVarint32Size(valueSize));
    if (iv) {
        memcpy(aesVector, iv, sizeof(aesVector));
    }
}

size_t KeyValueHolderCrypt::end() const {
    assert(flag == KeyValueHolderType_Offset);

    size_t size = offset;
    size += pbKeyValueSize + keySize + valueSize;
    return size;
}

// get decrypt data with [position, -1)
MMBuffer decryptBuffer(AESCrypt &crypter, const MMBuffer &inputBuffer, size_t position) {
    size_t length = inputBuffer.length();
    MMBuffer tmp(length);

    auto input = inputBuffer.getPtr();
    auto output = tmp.getPtr();
    crypter.decrypt(input, output, length);

    return tmp;
}

MMBuffer KeyValueHolderCrypt::toMMBuffer(const void *basePtr) const {
    if (flag == KeyValueHolderType_Direct) {
        return MMBuffer((void *) value, paddedSize, MMBufferNoCopy);
    } else {
        auto realPtr = (uint8_t *) basePtr + offset;
        realPtr += pbKeyValueSize + keySize + valueSize;
        return MMBuffer(realPtr, valueSize, MMBufferNoCopy);
    }
}

} // namespace mmkv
