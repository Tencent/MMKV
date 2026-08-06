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
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#ifdef MMKV_WIN32
#    include <wincrypt.h>
#    ifdef _MSC_VER
#        pragma comment(lib, "advapi32.lib")
#    endif
#else
#    include <fcntl.h>
#    include <unistd.h>
#endif
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

static void secureWipe(void *ptr, size_t len) noexcept;

AESCrypt::AESCrypt(const void *key, size_t keyLength, const void *iv, size_t ivLength) {
    if (key && keyLength > 0) {
        m_keyLength = (keyLength > AES_KEY_LEN) ? AES_KEY_LEN : keyLength;
        memcpy(m_key, key, m_keyLength);

        resetIV(iv, ivLength);

        m_aesKey = new AES_KEY;
        memset(m_aesKey, 0, sizeof(AES_KEY));
        int ret = AES_set_encrypt_key(m_key, AES_KEY_BITSET_LEN, m_aesKey);
        MMKV_ASSERT(ret == 0);
    }
}

AESCrypt::AESCrypt(const AESCrypt &other, const AESCryptStatus &status)
    : m_isClone(true), m_keyLength(other.m_keyLength), m_number(status.m_number) {
    memcpy(m_key, other.m_key, sizeof(m_key));
    memcpy(m_vector, status.m_vector, sizeof(m_vector));
    m_aesKey = other.m_aesKey;
}

AESCrypt::AESCrypt(AESCrypt &&other) noexcept
    : m_isClone(other.m_isClone)
    , m_keyLength(other.m_keyLength)
    , m_number(other.m_number)
    , m_aesKey(other.m_aesKey)
    , m_aesRollbackKey(other.m_aesRollbackKey) {
    memcpy(m_key, other.m_key, sizeof(m_key));
    memcpy(m_vector, other.m_vector, sizeof(m_vector));

    other.m_isClone = true;
    other.m_aesKey = nullptr;
    other.m_aesRollbackKey = nullptr;
    secureWipe(other.m_key, sizeof(other.m_key));
    secureWipe(other.m_vector, sizeof(other.m_vector));
    other.m_keyLength = 0;
    other.m_number = 0;
}

static void secureWipe(void *ptr, size_t len) noexcept {
    if (!ptr || len == 0) {
        return;
    }
#ifdef MMKV_WIN32
    SecureZeroMemory(ptr, len);
#else
    auto *bytes = static_cast<volatile unsigned char *>(ptr);
    while (len > 0) {
        *bytes++ = 0;
        --len;
    }
#endif
}

AESCrypt::~AESCrypt() {
    if (!m_isClone) {
        if (m_aesKey) {
            secureWipe(m_aesKey, sizeof(AES_KEY));
            delete m_aesKey;
        }
        if (m_aesRollbackKey) {
            secureWipe(m_aesRollbackKey, sizeof(AES_KEY));
            delete m_aesRollbackKey;
        }
    }
    secureWipe(m_key, sizeof(m_key));
    secureWipe(m_vector, sizeof(m_vector));
    m_keyLength = 0;
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

bool AESCrypt::isSameKey(const void *key, size_t keyLength) const {
    if (!key) {
        return false;
    }
    auto effectiveLength = (keyLength > AES_KEY_LEN) ? AES_KEY_LEN : keyLength;
    if (effectiveLength != m_keyLength) {
        return false;
    }
    auto input = static_cast<const uint8_t *>(key);
    uint8_t difference = 0;
    for (size_t index = 0; index < effectiveLength; index++) {
        difference |= static_cast<uint8_t>(m_key[index] ^ input[index]);
    }
    return difference == 0;
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

bool AESCrypt::fillRandomIV(void *vector) {
    if (!vector) {
        return false;
    }

#ifdef MMKV_WIN32
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContext(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        MMKVError("fail to acquire cryptographic provider, error=%lu", GetLastError());
        return false;
    }
    auto result = CryptGenRandom(provider, AES_KEY_LEN, static_cast<BYTE *>(vector)) == TRUE;
    if (!result) {
        MMKVError("fail to generate random IV, error=%lu", GetLastError());
    }
    CryptReleaseContext(provider, 0);
    return result;
#else
    auto fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        MMKVError("fail to open /dev/urandom, error=%d(%s)", errno, strerror(errno));
        return false;
    }

    size_t offset = 0;
    auto *bytes = static_cast<uint8_t *>(vector);
    while (offset < AES_KEY_LEN) {
        auto count = read(fd, bytes + offset, AES_KEY_LEN - offset);
        if (count > 0) {
            offset += static_cast<size_t>(count);
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            MMKVError("fail to read /dev/urandom, error=%d(%s)", errno, strerror(errno));
            break;
        }
    }
    close(fd);
    return offset == AES_KEY_LEN;
#endif
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
            size_t inputWord = 0;
            size_t outputWord = 0;
            memcpy(&inputWord, input + n, sizeof(inputWord));
            memcpy(&outputWord, output + n, sizeof(outputWord));
            auto vectorWord = inputWord ^ outputWord;
            memcpy(ivec + n, &vectorWord, sizeof(vectorWord));
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
