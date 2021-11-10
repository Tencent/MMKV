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

#include "CodedInputDataCrypt.h"
#include "MMKVLog.h"
#include "PBUtility.h"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <stdexcept>

#ifdef MMKV_APPLE
#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif
#endif // MMKV_APPLE

#ifndef MMKV_DISABLE_CRYPT

using namespace std;

namespace mmkv {

CodedInputDataCrypt::CodedInputDataCrypt(const void *oData, size_t length, AESCrypt &crypt)
    : m_ptr((uint8_t *) oData), m_size(length), m_position(0), m_decryptPosition(0), m_decrypter(crypt) {
    m_decryptBufferSize = AES_KEY_LEN * 2;
    m_decryptBufferPosition = static_cast<size_t>(crypt.m_number);
    m_decryptBufferDiscardPosition = m_decryptBufferPosition;
    m_decryptBufferDecryptLength = m_decryptBufferPosition;

    m_decryptBuffer = (uint8_t *) malloc(m_decryptBufferSize);
    if (!m_decryptBuffer) {
        throw runtime_error(strerror(errno));
    }
}

CodedInputDataCrypt::~CodedInputDataCrypt() {
    if (m_decryptBuffer) {
        free(m_decryptBuffer);
    }
}

void CodedInputDataCrypt::seek(size_t addedSize) {
    m_position += addedSize;
    m_decryptPosition += addedSize;

    if (m_position > m_size) {
        throw out_of_range("OutOfSpace");
    }
    assert(m_position % AES_KEY_LEN == m_decrypter.m_number);
}

void CodedInputDataCrypt::consumeBytes(size_t length, bool discardPreData) {
    if (discardPreData) {
        m_decryptBufferDiscardPosition = m_decryptBufferPosition;
    }
    auto decryptedBytesLeft = m_decryptBufferDecryptLength - m_decryptBufferPosition;
    if (decryptedBytesLeft >= length) {
        return;
    }
    length -= decryptedBytesLeft;

    // if there's some data left inside m_decrypter.m_vector, use them first
    // it will be faster when always decrypt with (n * AES_KEY_LEN) bytes
    if (m_decrypter.m_number != 0) {
        auto alignDecrypter = AES_KEY_LEN - m_decrypter.m_number;
        // make sure no data left inside m_decrypter.m_vector after decrypt
        if (length < alignDecrypter) {
            length = alignDecrypter;
        } else {
            length -= alignDecrypter;
            length = ((length + AES_KEY_LEN - 1) / AES_KEY_LEN) * AES_KEY_LEN;
            length += alignDecrypter;
        }
    } else {
        length = ((length + AES_KEY_LEN - 1) / AES_KEY_LEN) * AES_KEY_LEN;
    }
    auto bytesLeftInSrc = m_size - m_decryptPosition;
    length = min(bytesLeftInSrc, length);

    auto bytesLeftInBuffer = m_decryptBufferSize - m_decryptBufferDecryptLength;
    // try move some space
    if (bytesLeftInBuffer < length && m_decryptBufferDiscardPosition > 0) {
        auto posToMove = (m_decryptBufferDiscardPosition / AES_KEY_LEN) * AES_KEY_LEN;
        if (posToMove) {
            auto sizeToMove = m_decryptBufferDecryptLength - posToMove;
            memmove(m_decryptBuffer, m_decryptBuffer + posToMove, sizeToMove);
            m_decryptBufferPosition -= posToMove;
            m_decryptBufferDecryptLength -= posToMove;
            m_decryptBufferDiscardPosition = 0;
            bytesLeftInBuffer = m_decryptBufferSize - m_decryptBufferDecryptLength;
        }
    }
    // still no enough space, try realloc()
    if (bytesLeftInBuffer < length) {
        auto newSize = m_decryptBufferSize + length;
        auto newBuffer = realloc(m_decryptBuffer, newSize);
        if (!newBuffer) {
            throw runtime_error(strerror(errno));
        }
        m_decryptBuffer = (uint8_t *) newBuffer;
        m_decryptBufferSize = newSize;
    }
    m_decrypter.decrypt(m_ptr + m_decryptPosition, m_decryptBuffer + m_decryptBufferDecryptLength, length);
    m_decryptPosition += length;
    m_decryptBufferDecryptLength += length;
    assert(m_decryptPosition == m_size || m_decrypter.m_number == 0);
}

void CodedInputDataCrypt::skipBytes(size_t length) {
    m_position += length;

    auto decryptedBytesLeft = m_decryptBufferDecryptLength - m_decryptBufferPosition;
    if (decryptedBytesLeft >= length) {
        m_decryptBufferPosition += length;
        return;
    }
    length -= decryptedBytesLeft;
    // if this happens, we need optimization like the alignDecrypter above
    assert(m_decrypter.m_number == 0);

    size_t alignSize = ((length + AES_KEY_LEN - 1) / AES_KEY_LEN) * AES_KEY_LEN;
    auto bytesLeftInSrc = m_size - m_decryptPosition;
    auto size = min(alignSize, bytesLeftInSrc);
    decryptedBytesLeft = size - length;
    for (size_t index = 0, round = size / AES_KEY_LEN; index < round; index++) {
        m_decrypter.decrypt(m_ptr + m_decryptPosition, m_decryptBuffer, AES_KEY_LEN);
        m_decryptPosition += AES_KEY_LEN;
        size -= AES_KEY_LEN;
    }
    if (size) {
        m_decrypter.decrypt(m_ptr + m_decryptPosition, m_decryptBuffer, size);
        m_decryptPosition += size;
        m_decryptBufferPosition = size - decryptedBytesLeft;
        m_decryptBufferDecryptLength = size;
    } else {
        m_decryptBufferPosition = AES_KEY_LEN - decryptedBytesLeft;
        m_decryptBufferDecryptLength = AES_KEY_LEN;
    }
    assert(m_decryptBufferPosition <= m_decryptBufferDecryptLength);
    assert(m_decryptPosition - m_decryptBufferDecryptLength + m_decryptBufferPosition == m_position);
}

inline void CodedInputDataCrypt::statusBeforeDecrypt(size_t rollbackSize, AESCryptStatus &status) {
    rollbackSize += m_decryptBufferDecryptLength - m_decryptBufferPosition;
    m_decrypter.statusBeforeDecrypt(m_ptr + m_decryptPosition, m_decryptBuffer + m_decryptBufferDecryptLength,
                                    rollbackSize, status);
}

int8_t CodedInputDataCrypt::readRawByte() {
    assert(m_position <= m_decryptPosition);
    if (m_position == m_size) {
        auto msg = "reach end, m_position: " + to_string(m_position) + ", m_size: " + to_string(m_size);
        throw out_of_range(msg);
    }
    m_position++;

    assert(m_decryptBufferPosition < m_decryptBufferSize);
    auto *bytes = (int8_t *) m_decryptBuffer;
    return bytes[m_decryptBufferPosition++];
}

int32_t CodedInputDataCrypt::readRawVarint32(bool discardPreData) {
    consumeBytes(10, discardPreData);

    int8_t tmp = this->readRawByte();
    if (tmp >= 0) {
        return tmp;
    }
    int32_t result = tmp & 0x7f;
    if ((tmp = this->readRawByte()) >= 0) {
        result |= tmp << 7;
    } else {
        result |= (tmp & 0x7f) << 7;
        if ((tmp = this->readRawByte()) >= 0) {
            result |= tmp << 14;
        } else {
            result |= (tmp & 0x7f) << 14;
            if ((tmp = this->readRawByte()) >= 0) {
                result |= tmp << 21;
            } else {
                result |= (tmp & 0x7f) << 21;
                result |= (tmp = this->readRawByte()) << 28;
                if (tmp < 0) {
                    // discard upper 32 bits
                    for (int i = 0; i < 5; i++) {
                        if (this->readRawByte() >= 0) {
                            return result;
                        }
                    }
                    throw invalid_argument("InvalidProtocolBuffer malformed varint32");
                }
            }
        }
    }
    return result;
}

int32_t CodedInputDataCrypt::readInt32() {
    return this->readRawVarint32();
}

#    ifndef MMKV_APPLE

string CodedInputDataCrypt::readString(KeyValueHolderCrypt &kvHolder) {
    kvHolder.offset = static_cast<uint32_t>(m_position);

    int32_t size = this->readRawVarint32(true);
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        consumeBytes(s_size);

        kvHolder.keySize = static_cast<uint16_t>(s_size);

        string result((char *) (m_decryptBuffer + m_decryptBufferPosition), s_size);
        m_position += s_size;
        m_decryptBufferPosition += s_size;
        return result;
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

#    endif

void CodedInputDataCrypt::readData(KeyValueHolderCrypt &kvHolder) {
    int32_t size = this->readRawVarint32();
    if (size < 0) {
        throw length_error("InvalidProtocolBuffer negativeSize");
    }

    auto s_size = static_cast<size_t>(size);
    if (s_size <= m_size - m_position) {
        if (KeyValueHolderCrypt::isValueStoredAsOffset(s_size)) {
            kvHolder.type = KeyValueHolderType_Offset;
            kvHolder.valueSize = static_cast<uint32_t>(s_size);
            kvHolder.pbKeyValueSize =
                static_cast<uint8_t>(pbRawVarint32Size(kvHolder.valueSize) + pbRawVarint32Size(kvHolder.keySize));

            size_t rollbackSize = kvHolder.pbKeyValueSize + kvHolder.keySize;
            statusBeforeDecrypt(rollbackSize, kvHolder.cryptStatus);

            skipBytes(s_size);
        } else {
            consumeBytes(s_size);

            kvHolder.type = KeyValueHolderType_Direct;
            kvHolder = KeyValueHolderCrypt(m_decryptBuffer + m_decryptBufferPosition, s_size);
            m_decryptBufferPosition += s_size;
            m_position += s_size;
        }
    } else {
        throw out_of_range("InvalidProtocolBuffer truncatedMessage");
    }
}

} // namespace mmkv

#endif // MMKV_DISABLE_CRYPT
