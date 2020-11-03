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
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace mmkv {

KeyValueHolder::KeyValueHolder(uint32_t keyLength, uint32_t valueLength, uint32_t off)
    : keySize(static_cast<uint16_t>(keyLength)), valueSize(valueLength), offset(off) {
    computedKVSize = keySize + static_cast<uint16_t>(pbRawVarint32Size(keySize));
    computedKVSize += static_cast<uint16_t>(pbRawVarint32Size(valueSize));
}

MMBuffer KeyValueHolder::toMMBuffer(const void *basePtr) const {
    auto realPtr = (uint8_t *) basePtr + offset;
    realPtr += computedKVSize;
    return MMBuffer(realPtr, valueSize, MMBufferNoCopy);
}

#ifndef MMKV_DISABLE_CRYPT

KeyValueHolderCrypt::KeyValueHolderCrypt(const void *src, size_t length) {
    if (length <= SmallBufferSize()) {
        type = KeyValueHolderType_Direct;
        paddedSize = static_cast<uint8_t>(length);
        memcpy(paddedValue, src, length);
    } else {
        type = KeyValueHolderType_Memory;
        memSize = static_cast<uint32_t>(length);
        memPtr = malloc(length);
        if (!memPtr) {
            throw std::runtime_error(strerror(errno));
        }
        memcpy(memPtr, src, memSize);
    }
}

KeyValueHolderCrypt::KeyValueHolderCrypt(MMBuffer &&data) {
    if (data.type == MMBuffer::MMBufferType_Small) {
        static_assert(SmallBufferSize() >= MMBuffer::SmallBufferSize(), "KeyValueHolderCrypt can't hold MMBuffer");

        type = KeyValueHolderType_Direct;
        paddedSize = static_cast<uint8_t>(data.length());
        memcpy(paddedValue, data.getPtr(), data.length());
    } else {
        type = KeyValueHolderType_Memory;
        memSize = static_cast<uint32_t>(data.length());
#    ifdef MMKV_APPLE
        if (data.m_data != nil) {
            memPtr = malloc(memSize);
            if (!memPtr) {
                throw std::runtime_error(strerror(errno));
            }
            memcpy(memPtr, data.getPtr(), memSize);
            return;
        }
#    endif
        memPtr = data.getPtr();
        data.detach();
    }
}

KeyValueHolderCrypt::KeyValueHolderCrypt(uint32_t keyLength, uint32_t valueLength, uint32_t off)
    : type(KeyValueHolderType_Offset), keySize(static_cast<uint16_t>(keyLength)), valueSize(valueLength), offset(off) {

    pbKeyValueSize = static_cast<uint8_t>(pbRawVarint32Size(keySize) + pbRawVarint32Size(valueSize));
}

KeyValueHolderCrypt::KeyValueHolderCrypt(KeyValueHolderCrypt &&other) noexcept {
    this->move(std::move(other));
}

KeyValueHolderCrypt &KeyValueHolderCrypt::operator=(KeyValueHolderCrypt &&other) noexcept {
    if (type == KeyValueHolderType_Memory && memPtr) {
        free(memPtr);
    }
    this->move(std::move(other));
    return *this;
}

void KeyValueHolderCrypt::move(KeyValueHolderCrypt &&other) noexcept {
    if (other.type == KeyValueHolderType_Direct || other.type == KeyValueHolderType_Offset) {
        memcpy(this, &other, sizeof(other));
    } else if (other.type == KeyValueHolderType_Memory) {
        type = KeyValueHolderType_Memory;
        memSize = other.memSize;
        memPtr = other.memPtr;
        other.memPtr = nullptr;
    }
}

KeyValueHolderCrypt::~KeyValueHolderCrypt() {
    if (type == KeyValueHolderType_Memory && memPtr) {
        free(memPtr);
    }
}

uint32_t KeyValueHolderCrypt::realValueSize() const {
    switch (type) {
        case KeyValueHolderType_Direct:
            return paddedSize;
        case KeyValueHolderType_Offset:
            return valueSize;
        case KeyValueHolderType_Memory:
            return memSize;
    }
    return 0;
}

// get decrypt data with [position, -1)
static MMBuffer decryptBuffer(AESCrypt &crypter, const MMBuffer &inputBuffer, size_t position) {
    static size_t smallBuffer[16 / sizeof(size_t)];
    auto basePtr = (uint8_t *) inputBuffer.getPtr();
    auto ptr = basePtr;
    for (size_t index = sizeof(smallBuffer); index < position; index += sizeof(smallBuffer)) {
        crypter.decrypt(ptr, smallBuffer, sizeof(smallBuffer));
        ptr += sizeof(smallBuffer);
    }
    if (ptr < basePtr + position) {
        crypter.decrypt(ptr, smallBuffer, static_cast<size_t>(basePtr + position - ptr));
        ptr = basePtr + position;
    }
    size_t length = inputBuffer.length() - position;
    MMBuffer tmp(length);

    auto input = ptr;
    auto output = tmp.getPtr();
    crypter.decrypt(input, output, length);

    return tmp;
}

MMBuffer KeyValueHolderCrypt::toMMBuffer(const void *basePtr, const AESCrypt *crypter) const {
    if (type == KeyValueHolderType_Direct) {
        return MMBuffer((void *) paddedValue, paddedSize, MMBufferNoCopy);
    } else if (type == KeyValueHolderType_Memory) {
        return MMBuffer(memPtr, memSize, MMBufferNoCopy);
    } else {
        auto realPtr = (uint8_t *) basePtr + offset;
        auto position = static_cast<uint32_t>(pbKeyValueSize + keySize);
        auto realSize = position + valueSize;
        auto kvBuffer = MMBuffer(realPtr, realSize, MMBufferNoCopy);
        auto decrypter = crypter->cloneWithStatus(cryptStatus);
        return decryptBuffer(decrypter, kvBuffer, position);
    }
}

#endif // MMKV_DISABLE_CRYPT

} // namespace mmkv

#if !defined(MMKV_DISABLE_CRYPT) && defined(MMKV_DEBUG)
#    include "CodedInputData.h"
#    include "CodedOutputData.h"
#    include "MMKVLog.h"
#    include <ctime>

using namespace std;

namespace mmkv {

void KeyValueHolderCrypt::testAESToMMBuffer() {
    const uint8_t plainText[] = "Hello, OpenSSL-mmkv::KeyValueHolderCrypt::testAESToMMBuffer() with AES CFB 128.";
    constexpr size_t textLength = sizeof(plainText) - 1;

    const uint8_t key[] = "TheAESKey";
    constexpr size_t keyLength = sizeof(key) - 1;

    uint8_t iv[AES_KEY_LEN];
    srand((unsigned) time(nullptr));
    for (uint32_t i = 0; i < AES_KEY_LEN; i++) {
        iv[i] = (uint8_t) rand();
    }
    AESCrypt crypt1(key, keyLength, iv, sizeof(iv));

    auto encryptText = new uint8_t[DEFAULT_MMAP_SIZE];
    memset(encryptText, 0, DEFAULT_MMAP_SIZE);
    CodedOutputData output(encryptText, DEFAULT_MMAP_SIZE);
    output.writeData(MMBuffer((void *) key, keyLength, MMBufferNoCopy));
    auto lengthOfValue = textLength + pbRawVarint32Size((uint32_t) textLength);
    output.writeRawVarint32((int32_t) lengthOfValue);
    output.writeData(MMBuffer((void *) plainText, textLength, MMBufferNoCopy));
    crypt1.encrypt(encryptText, encryptText, (size_t)(output.curWritePointer() - encryptText));

    AESCrypt decrypt(key, keyLength, iv, sizeof(iv));
    uint8_t smallBuffer[32];
    decrypt.decrypt(encryptText, smallBuffer, 5);
    auto keySize = CodedInputData(smallBuffer, 5).readUInt32();
    auto sizeOfKeySize = pbRawVarint32Size(keySize);
    auto position = sizeOfKeySize;
    decrypt.decrypt(encryptText + 5, smallBuffer + 5, static_cast<size_t>(sizeOfKeySize + keySize - 5));
    position += keySize;
    decrypt.decrypt(encryptText + position, smallBuffer + position, 5);
    auto valueSize = CodedInputData(smallBuffer + position, 5).readUInt32();
    // auto sizeOfValueSize = pbRawVarint32Size(valueSize);
    KeyValueHolderCrypt kvHolder(keySize, valueSize, 0);
    auto rollbackSize = position + 5;
    decrypt.statusBeforeDecrypt(encryptText + rollbackSize, smallBuffer + rollbackSize, rollbackSize,
                                kvHolder.cryptStatus);
    auto value = kvHolder.toMMBuffer(encryptText, &decrypt);
#    ifdef MMKV_APPLE
    MMKVInfo("testAESToMMBuffer: %@", CodedInputData((char *) value.getPtr(), value.length()).readString());
#    else
    MMKVInfo("testAESToMMBuffer: %s", CodedInputData((char *) value.getPtr(), value.length()).readString().c_str());
#    endif
    MMKVInfo("MMBuffer::SmallBufferSize() = %u, KeyValueHolderCrypt::SmallBufferSize() = %u",
             MMBuffer::SmallBufferSize(), KeyValueHolderCrypt::SmallBufferSize());

    delete[] encryptText;
}

} // namespace mmkv

#endif
