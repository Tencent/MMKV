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

#ifndef CodedInputDataCrypt_h
#define CodedInputDataCrypt_h
#ifdef __cplusplus

#include "MMKVPredef.h"

#include "KeyValueHolder.h"
#include "MMBuffer.h"
#include "aes/AESCrypt.h"
#include <cstdint>

#ifdef MMKV_DISABLE_CRYPT

namespace mmkv {
class CodedInputDataCrypt;
}

#else

namespace mmkv {

class CodedInputDataCrypt {
    uint8_t *const m_ptr;
    size_t m_size;
    size_t m_position;
    size_t m_decryptPosition; // position of text that has beed decrypted

    AESCrypt &m_decrypter;
    uint8_t *m_decryptBuffer; // internal decrypt buffer, grows by (n * AES_KEY_LEN) bytes
    size_t m_decryptBufferSize;
    size_t m_decryptBufferPosition; // reader position in the buffer, synced with m_position
    size_t m_decryptBufferDecryptLength; // length of the buffer that has been used
    size_t m_decryptBufferDiscardPosition; // recycle position, any data before that can be discarded

    void consumeBytes(size_t length, bool discardPreData = false);
    void skipBytes(size_t length);
    void statusBeforeDecrypt(size_t rollbackSize, AESCryptStatus &status);

    int8_t readRawByte();

    int32_t readRawVarint32(bool discardPreData = false);

public:
    CodedInputDataCrypt(const void *oData, size_t length, AESCrypt &crypt);

    ~CodedInputDataCrypt();

    bool isAtEnd() { return m_position == m_size; };

    void seek(size_t addedSize);

    int32_t readInt32();

    void readData(KeyValueHolderCrypt &kvHolder);

#ifndef MMKV_APPLE
    std::string readString(KeyValueHolderCrypt &kvHolder);
#else
    NSString *readString(KeyValueHolderCrypt &kvHolder);
#endif
};

} // namespace mmkv

#endif // MMKV_DISABLE_CRYPT
#endif // __cplusplus
#endif /* CodedInputDataCrypt_h */
