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

#include "AESCrypt.h"
#include "openssl/openssl_aes.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdint.h>

using namespace openssl;

namespace mmkv {

AESCrypt::AESCrypt(const void *key, size_t keyLength, const void *iv, size_t ivLength) {
    if (key && keyLength > 0) {
        memcpy(m_key, key, (keyLength > AES_KEY_LEN) ? AES_KEY_LEN : keyLength);

        resetIV(iv, ivLength);

        m_aesKey = new AES_KEY;
        memset(m_aesKey, 0, sizeof(AES_KEY));
        int ret = AES_set_encrypt_key(m_key, AES_KEY_BITSET_LEN, m_aesKey);
        MMKV_ASSERT(ret == 0);
    }
}

AESCrypt::~AESCrypt() {
    delete m_aesKey;
    m_aesKey = nullptr;
}

void AESCrypt::resetIV(const void *iv, size_t ivLength) {
    m_number = 0;
    if (iv && ivLength > 0) {
        memcpy(m_vector, iv, (ivLength > AES_KEY_LEN) ? AES_KEY_LEN : ivLength);
    } else {
        memcpy(m_vector, m_key, AES_KEY_LEN);
    }
}

void AESCrypt::getKey(void *output) const {
    if (output) {
        memcpy(output, m_key, AES_KEY_LEN);
    }
}

void AESCrypt::encrypt(const void *input, void *output, size_t length) {
    if (!input || !output || length == 0) {
        return;
    }
    AES_cfb128_encrypt((const unsigned char *) input, (unsigned char *) output, length, m_aesKey, m_vector, &m_number);
}

void AESCrypt::decrypt(const void *input, void *output, size_t length) {
    if (!input || !output || length == 0) {
        return;
    }
    AES_cfb128_decrypt((const unsigned char *) input, (unsigned char *) output, length, m_aesKey, m_vector, &m_number);
}

void AESCrypt::fillRandomIV(void *vector) {
    if (!vector) {
        return;
    }
    srand((unsigned) time(nullptr));
    int *ptr = (int *) vector;
    for (uint32_t i = 0; i < AES_KEY_LEN / sizeof(int); i++) {
        ptr[i] = rand();
    }
}

} // namespace mmkv

#ifndef NDEBUG

#    include "../MMKVLog.h"
#    include "../MemoryFile.h"

namespace mmkv {

// check if AESCrypt is encrypt-decrypt full-duplex
void testAESCrypt() {
    const unsigned char plainText[] = "Hello, OpenSSL with AES CFB 128.";
    constexpr size_t textLength = sizeof(plainText) - 1;

    const unsigned char key[] = "TheAESKey";
    constexpr size_t keyLength = sizeof(key) - 1;

    unsigned char iv[AES_KEY_LEN];
    srand((unsigned) time(nullptr));
    for (uint32_t i = 0; i < AES_KEY_LEN; i++) {
        iv[i] = (unsigned char) rand();
    }
    AESCrypt crypt1(key, keyLength, iv, sizeof(iv));
    AESCrypt crypt2(key, keyLength, iv, sizeof(iv));

    auto encryptText = new unsigned char[DEFAULT_MMAP_SIZE];
    auto decryptText = new unsigned char[DEFAULT_MMAP_SIZE];
    memset(encryptText, 0, DEFAULT_MMAP_SIZE);
    memset(decryptText, 0, DEFAULT_MMAP_SIZE);

    /* in-place encryption & decryption, this is crazy
    memcpy(encryptText, plainText, textLength);
    crypt1.encrypt(encryptText, encryptText, textLength);
    crypt2.decrypt(encryptText, encryptText, textLength);
    return;
    */

    size_t actualSize = 0;
    bool flip = false;
    for (const unsigned char *ptr = plainText; ptr < plainText + textLength;) {
        auto tokenPtr = (const unsigned char *) strchr((const char *) ptr, ' ');
        size_t size = 0;
        if (!tokenPtr) {
            size = static_cast<size_t>(plainText + textLength - ptr);
        } else {
            size = static_cast<size_t>(tokenPtr - ptr + 1);
        }

        flip = !flip;
        if (flip) {
            crypt1.encrypt(plainText + actualSize, encryptText + actualSize, size);
            crypt2.decrypt(encryptText + actualSize, decryptText + actualSize, size);
        } else {
            crypt2.encrypt(plainText + actualSize, encryptText + actualSize, size);
            crypt1.decrypt(encryptText + actualSize, decryptText + actualSize, size);
        }

        actualSize += size;
        ptr += size;
    }
    MMKVDebug("AES CFB decode: %s", decryptText);

    delete[] encryptText;
    delete[] decryptText;
}

} // namespace mmkv

#endif
