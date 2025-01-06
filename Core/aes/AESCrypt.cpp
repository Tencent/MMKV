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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "../MMKVLog.h"
#include "../MemoryFile.h"

namespace mmkv {

// assuming size in [1, 5]
uint32_t AESCrypt::randomItemSizeHolder(uint32_t size) {
    constexpr uint32_t ItemSizeHolders[] = {0, 0x80, 0x4000, 0x200000, 0x10000000, 0};
    auto ItemSizeHolderMin = ItemSizeHolders[size - 1];
    auto ItemSizeHolderMax = ItemSizeHolders[size] - 1;

    srand((unsigned) time(nullptr));
    auto result = static_cast<uint32_t>(rand());
    result = result % (ItemSizeHolderMax - ItemSizeHolderMin + 1);
    result += ItemSizeHolderMin;
    return result;
}

#ifndef MMKV_DISABLE_CRYPT

using namespace openssl;

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

AESCrypt::AESCrypt(const AESCrypt &other, const AESCryptStatus &status) : m_isClone(true), m_number(status.m_number) {
    //memcpy(m_key, other.m_key, sizeof(m_key));
    memcpy(m_vector, status.m_vector, sizeof(m_vector));
    m_aesKey = other.m_aesKey;
}

AESCrypt::~AESCrypt() {
    if (!m_isClone) {
        delete m_aesKey;
        delete m_aesRollbackKey;
    }
}

void AESCrypt::resetIV(const void *iv, size_t ivLength) {
    m_number = 0;
    if (iv && ivLength > 0) {
        memcpy(m_vector, iv, (ivLength > AES_KEY_LEN) ? AES_KEY_LEN : ivLength);
    } else {
        memcpy(m_vector, m_key, AES_KEY_LEN);
    }
}

void AESCrypt::resetStatus(const AESCryptStatus &status) {
    m_number = status.m_number;
    memcpy(m_vector, status.m_vector, AES_KEY_LEN);
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
    AES_cfb128_encrypt((const uint8_t *) input, (uint8_t *) output, length, m_aesKey, m_vector, &m_number);
}

void AESCrypt::decrypt(const void *input, void *output, size_t length) {
    if (!input || !output || length == 0) {
        return;
    }
    AES_cfb128_decrypt((const uint8_t *) input, (uint8_t *) output, length, m_aesKey, m_vector, &m_number);
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

static inline void
Rollback_cfb_decrypt(const uint8_t *input, const uint8_t *output, size_t len, AES_KEY *key, AESCryptStatus &status) {
    auto ivec = status.m_vector;
    auto n = status.m_number;

    while (n && len) {
        auto c = *(--output);
        ivec[--n] = *(--input) ^ c;
        len--;
    }
    if (n == 0 && (status.m_number != 0)) {
        AES_decrypt(ivec, ivec, key);
    }
    while (len >= 16) {
        len -= 16;
        output -= 16;
        input -= 16;
        for (; n < 16; n += sizeof(size_t)) {
            size_t t = *(size_t *) (output + n);
            *(size_t *) (ivec + n) = *(size_t *) (input + n) ^ t;
        }
        n = 0;
        AES_decrypt(ivec, ivec, key);
    }
    if (len) {
        n = 16;
        do {
            auto c = *(--output);
            ivec[--n] = *(--input) ^ c;
            len--;
        } while (len);
    }

    status.m_number = n;
}

void AESCrypt::statusBeforeDecrypt(const void *input, const void *output, size_t length, AESCryptStatus &status) {
    if (length == 0) {
        return;
    }
    if (!m_aesRollbackKey) {
        m_aesRollbackKey = new AES_KEY;
        memset(m_aesRollbackKey, 0, sizeof(AES_KEY));
        int ret = AES_set_decrypt_key(m_key, AES_KEY_BITSET_LEN, m_aesRollbackKey);
        MMKV_ASSERT(ret == 0);
    }
    getCurStatus(status);
    Rollback_cfb_decrypt((const uint8_t *) input, (const uint8_t *) output, length, m_aesRollbackKey, status);
}

void AESCrypt::getCurStatus(AESCryptStatus &status) {
    status.m_number = static_cast<uint8_t>(m_number);
    memcpy(status.m_vector, m_vector, sizeof(m_vector));
}

AESCrypt AESCrypt::cloneWithStatus(const AESCryptStatus &status) const {
    return AESCrypt(*this, status);
}

#    ifdef MMKV_DEBUG

void testRandomPlaceHolder() {
    for (uint32_t size = 1; size < 6; size++) {
        auto holder = AESCrypt::randomItemSizeHolder(size);
        MMKVInfo("holder 0x%x for size %u", holder, size);
    }
}

// check if AESCrypt is encrypt-decrypt full-duplex
void AESCrypt::testAESCrypt() {
    testRandomPlaceHolder();

    const uint8_t plainText[] = "Hello, OpenSSL-mmkv::AESCrypt::testAESCrypt() with AES CFB 128.";
    constexpr size_t textLength = sizeof(plainText) - 1;

    const uint8_t key[] = "TheAESKey";
    constexpr size_t keyLength = sizeof(key) - 1;

    uint8_t iv[AES_KEY_LEN];
    srand((unsigned) time(nullptr));
    for (uint32_t i = 0; i < AES_KEY_LEN; i++) {
        iv[i] = (uint8_t) rand();
    }
    AESCrypt crypt1(key, keyLength, iv, sizeof(iv));
    AESCrypt crypt2(key, keyLength, iv, sizeof(iv));

    auto encryptText = new uint8_t[DEFAULT_MMAP_SIZE];
    auto decryptText = new uint8_t[DEFAULT_MMAP_SIZE];
    memset(encryptText, 0, DEFAULT_MMAP_SIZE);
    memset(decryptText, 0, DEFAULT_MMAP_SIZE);

    /* in-place encryption & decryption
    memcpy(encryptText, plainText, textLength);
    crypt1.encrypt(encryptText, encryptText, textLength);
    crypt2.decrypt(encryptText, encryptText, textLength);
    return;
    */
    AES_KEY decryptKey;
    AES_set_decrypt_key(crypt1.m_key, AES_KEY_BITSET_LEN, &decryptKey);

    size_t actualSize = 0;
    bool flip = false;
    for (const uint8_t *ptr = plainText; ptr < plainText + textLength;) {
        auto tokenPtr = (const uint8_t *) strchr((const char *) ptr, ' ');
        size_t size = 0;
        if (!tokenPtr) {
            size = static_cast<size_t>(plainText + textLength - ptr);
        } else {
            size = static_cast<size_t>(tokenPtr - ptr + 1);
        }

        AESCrypt *decrypter;
        uint32_t oldNum;
        uint8_t oldVector[sizeof(crypt1.m_vector)];

        flip = !flip;
        if (flip) {
            crypt1.encrypt(plainText + actualSize, encryptText + actualSize, size);

            decrypter = &crypt2;
            oldNum = decrypter->m_number;
            memcpy(oldVector, decrypter->m_vector, sizeof(oldVector));
            crypt2.decrypt(encryptText + actualSize, decryptText + actualSize, size);
        } else {
            crypt2.encrypt(plainText + actualSize, encryptText + actualSize, size);

            decrypter = &crypt1;
            oldNum = decrypter->m_number;
            memcpy(oldVector, decrypter->m_vector, sizeof(oldVector));
            crypt1.decrypt(encryptText + actualSize, decryptText + actualSize, size);
        }
        // that's why AESCrypt can be full-duplex
        assert(crypt1.m_number == crypt2.m_number);
        assert(0 == memcmp(crypt1.m_vector, crypt2.m_vector, sizeof(crypt1.m_vector)));

        // how rollback works
        AESCryptStatus status;
        decrypter->statusBeforeDecrypt(encryptText + actualSize + size, decryptText + actualSize + size, size, status);
        assert(oldNum == status.m_number);
        assert(0 == memcmp(oldVector, status.m_vector, sizeof(oldVector)));

        actualSize += size;
        ptr += size;
    }
    MMKVInfo("AES CFB decode: %s", decryptText);

    delete[] encryptText;
    delete[] decryptText;
}

#    endif // MMKV_DEBUG
#endif     // MMKV_DISABLE_CRYPT

} // namespace mmkv
