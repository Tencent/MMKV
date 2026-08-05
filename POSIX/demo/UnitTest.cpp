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

#include <MMKV/MMKV.h>
#include "CodedOutputData.h"
#include "aes/AESCrypt.h"
#include "MemoryFile.h"
#include "MMKVMetaInfo.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <limits>
#include <numeric>
#include <new>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace std;
using namespace mmkv;
namespace fs = std::filesystem;

extern unordered_map<string, MMKV *> *g_instanceDic;

static const string KeyNotExist = "KeyNotExist";

void testBool(MMKV *mmkv) {
    auto ret = mmkv->set(true, "bool");
    assert(ret);

    auto value = mmkv->getBool("bool");
    assert(value);

    value = mmkv->getBool(KeyNotExist);
    assert(!value);

    value = mmkv->getBool(KeyNotExist, true);
    assert(value);

    printf("test bool: passed\n");
}

void testInt32(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<int32_t>::max(), "int32");
    assert(ret);

    auto value = mmkv->getInt32("int32");
    assert(value == numeric_limits<int32_t>::max());

    value = mmkv->getInt32(KeyNotExist);
    assert(value == 0);

    value = mmkv->getInt32(KeyNotExist, -1);
    assert(value == -1);

    printf("test int32: passed\n");
}

void testUInt32(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<uint32_t>::max(), "uint32");
    assert(ret);

    auto value = mmkv->getUInt32("uint32");
    assert(value == numeric_limits<uint32_t>::max());

    value = mmkv->getUInt32(KeyNotExist);
    assert(value == 0);

    value = mmkv->getUInt32(KeyNotExist, -1);
    assert(value == -1);

    printf("test uint32: passed\n");
}

void testInt64(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<int64_t>::min(), "int64");
    assert(ret);

    auto value = mmkv->getInt64("int64");
    assert(value == numeric_limits<int64_t>::min());

    value = mmkv->getInt64(KeyNotExist);
    assert(value == 0);

    value = mmkv->getInt64(KeyNotExist, -1);
    assert(value == -1);

    printf("test int64: passed\n");
}

void testUInt64(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<uint64_t>::max(), "uint64");
    assert(ret);

    auto value = mmkv->getUInt64("uint64");
    assert(value == numeric_limits<uint64_t>::max());

    value = mmkv->getUInt64(KeyNotExist);
    assert(value == 0);

    value = mmkv->getUInt64(KeyNotExist, -1);
    assert(value == -1);

    printf("test uint64: passed\n");
}

template <typename T>
bool EqualWithAccuracy(T value1, T value2, T accuracy) {
    return fabs(value1 - value2) <= accuracy;
}

void testFloat(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<float>::max(), "float");
    assert(ret);

    auto value = mmkv->getFloat("float");
    assert(EqualWithAccuracy(value, numeric_limits<float>::max(), 0.001f));

    value = mmkv->getFloat(KeyNotExist);
    assert(EqualWithAccuracy(value, 0.0f, 0.001f));

    value = mmkv->getFloat(KeyNotExist, -1.0f);
    assert(EqualWithAccuracy(value, -1.0f, 0.001f));

    printf("test float: passed\n");
}

void testDouble(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<double>::max(), "double");
    assert(ret);

    auto value = mmkv->getDouble("double");
    assert(EqualWithAccuracy(value, numeric_limits<double>::max(), 0.001));

    value = mmkv->getDouble(KeyNotExist);
    assert(EqualWithAccuracy(value, 0.0, 0.001));

    value = mmkv->getDouble(KeyNotExist, -1.0);
    assert(EqualWithAccuracy(value, -1.0, 0.001));

    printf("test double: passed\n");
}

void testString(MMKV *mmkv) {
    string str = "Hello 2018 world cup 世界杯";
    auto ret = mmkv->set(str, "string");
    assert(ret);

    string value;
    ret = mmkv->getString("string", value);
    assert(ret && str == value);

    const char *cString = "Hello 2022 world cup 世界杯";
    ret = mmkv->set(cString, "cstring");
    assert(ret);

    ret = mmkv->getString("cstring", value);
    assert(ret && value == cString);

    ret = mmkv->getString(KeyNotExist, value);
    assert(!ret);

    printf("test string: passed\n");
}

void testBytes(MMKV *mmkv) {
    string str = "Hello 2018 world cup 世界杯";
    MMBuffer buffer((void *) str.data(), str.length(), MMBufferNoCopy);
    auto ret = mmkv->set(buffer, "bytes");
    assert(ret);

    auto value = mmkv->getBytes("bytes");
    assert(value.length() == buffer.length() && memcmp(value.getPtr(), buffer.getPtr(), value.length()) == 0);

    value = mmkv->getBytes(KeyNotExist);
    assert(value.length() == 0);

    printf("test bytes: passed\n");
}

void testVector(MMKV *mmkv) {
    vector<string> v = {"1", "0", "2", "4"};
    auto ret = mmkv->set(v, "vector");
    assert(ret);

    vector<string> value;
    ret = mmkv->getVector("vector", value);
    assert(ret && value == v);

    printf("test vector: passed\n");
}

void testOversizedKey(MMKV *mmkv) {
    // Keys at or below 65531 bytes should work normally
    {
        string key(65531, 'A');
        auto ret = mmkv->set("V", key);
        assert(ret);
        string out;
        ret = mmkv->getString(key, out);
        assert(ret && out == "V");
        mmkv->removeValueForKey(key);
    }
    // Keys at 65532 bytes and above must be rejected (would overflow uint16_t fields)
    {
        string key(65532, 'B');
        auto ret = mmkv->set("V", key);
        assert(!ret);
        string out;
        ret = mmkv->getString(key, out);
        assert(!ret);
    }
    {
        string key(70000, 'C');
        auto ret = mmkv->set("V", key);
        assert(!ret);
    }

    printf("test oversized key: passed\n");
}

void testTransactionalOutputAndOversizedValue(MMKV *mmkv) {
    array<uint8_t, 8> storage;
    storage.fill(0xA5);

    CodedOutputData fixedOutput(storage.data(), 1);
    bool rejected = false;
    try {
        fixedOutput.writeRawLittleEndian32(0x12345678);
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected);
    assert(fixedOutput.getPosition() == 0);
    assert(storage[0] == 0xA5);
    fixedOutput.writeBool(true);
    assert(fixedOutput.getPosition() == 1 && storage[0] == 1);

    storage.fill(0xA5);
    CodedOutputData varintOutput(storage.data(), 1);
    rejected = false;
    try {
        varintOutput.writeUInt32(128);
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected);
    assert(varintOutput.getPosition() == 0 && storage[0] == 0xA5);

    CodedOutputData positionOutput(storage.data(), storage.size());
    positionOutput.seek(2);
    rejected = false;
    try {
        positionOutput.seek(numeric_limits<size_t>::max());
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && positionOutput.getPosition() == 2);
    rejected = false;
    try {
        positionOutput.setPosition(storage.size() + 1);
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && positionOutput.getPosition() == 2);

    if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
        uint8_t sentinel = 0x7B;
        auto oversizedLength = static_cast<size_t>(numeric_limits<uint32_t>::max()) + 1;
        MMBuffer oversized(&sentinel, oversizedLength, MMBufferNoCopy);

        storage.fill(0xA5);
        CodedOutputData dataOutput(storage.data(), storage.size());
        rejected = false;
        try {
            dataOutput.writeData(oversized);
        } catch (const length_error &) {
            rejected = true;
        }
        assert(rejected);
        assert(dataOutput.getPosition() == 0 && storage[0] == 0xA5);

        assert(!mmkv->set(oversized, "oversized-value"));
        assert(mmkv->set("still-writable", "after-oversized-value"));
        string value;
        assert(mmkv->getString("after-oversized-value", value));
        assert(value == "still-writable");
    }

    printf("test transactional output and oversized value: passed\n");
}

void testExpirationOverflow() {
    auto mmkv = MMKV::mmkvWithID("expiration_overflow_test");
    mmkv->clearAll();
    assert(mmkv->enableAutoKeyExpire(numeric_limits<uint32_t>::max()));

    auto ret = mmkv->set(true, "expiration_overflow_auto");
    assert(ret);
    assert(mmkv->getBool("expiration_overflow_auto"));

    ret = mmkv->set("manual", "expiration_overflow_manual", numeric_limits<uint32_t>::max());
    assert(ret);
    string value;
    ret = mmkv->getString("expiration_overflow_manual", value);
    assert(ret && value == "manual");

    string bytes = "bytes";
    MMBuffer buffer((void *) bytes.data(), bytes.length(), MMBufferNoCopy);
    ret = mmkv->set(buffer, "expiration_overflow_bytes", numeric_limits<uint32_t>::max());
    assert(ret);
    auto out = mmkv->getBytes("expiration_overflow_bytes");
    assert(out.length() == buffer.length() && memcmp(out.getPtr(), buffer.getPtr(), out.length()) == 0);

    assert(mmkv->count(true) == 3);
    mmkv->clearAll();

    printf("test expiration overflow: passed\n");
}

#ifndef MMKV_DISABLE_CRYPT
static bool containsBytes(const unsigned char *storage, size_t storageSize, const uint8_t *value, size_t valueSize) {
    if (valueSize > storageSize) {
        return false;
    }
    for (size_t offset = 0; offset <= storageSize - valueSize; ++offset) {
        if (memcmp(storage + offset, value, valueSize) == 0) {
            return true;
        }
    }
    return false;
}

static MMKVMetaInfo readMetaInfo(const string &path) {
    auto fd = open(path.c_str(), O_RDONLY);
    assert(fd >= 0);
    MMKVMetaInfo metaInfo;
    assert(readFileContent(fd, &metaInfo, sizeof(metaInfo)));
    close(fd);
    return metaInfo;
}

void testCryptoRandomAndWipe(const string &rootDir) {
    uint8_t firstIV[AES_IV_LEN] = {};
    uint8_t secondIV[AES_IV_LEN] = {};
    assert(AESCrypt::fillRandomIV(firstIV));
    assert(AESCrypt::fillRandomIV(secondIV));
    assert(memcmp(firstIV, secondIV, sizeof(firstIV)) != 0);

    uint8_t key[AES256_KEY_LEN];
    iota(begin(key), end(key), 1);
    alignas(AESCrypt) unsigned char storage[sizeof(AESCrypt)] = {};
    auto crypt = new (storage) AESCrypt(key, sizeof(key), nullptr, 0, true);
    assert(containsBytes(storage, sizeof(storage), key, sizeof(key)));
    crypt->~AESCrypt();
    assert(!containsBytes(storage, sizeof(storage), key, sizeof(key)));

    // Moving an owning crypter must transfer, rather than duplicate, ownership
    // of its allocated OpenSSL key schedules.
    {
        AESCrypt original(key, sizeof(key), nullptr, 0, true);
        AESCrypt moved(std::move(original));
        assert(moved.isSameKey(key, sizeof(key), true));
    }

    const string exactAES128Key = "0123456789abcdef";
    auto encryptedMMKV = MMKV::mmkvWithID("aes128-crypt-key", MMKV_SINGLE_PROCESS, &exactAES128Key);
    assert(encryptedMMKV);
    assert(encryptedMMKV->cryptKey() == exactAES128Key);
    encryptedMMKV->clearAll();
    encryptedMMKV->close();

    const char binaryKeyBytes[] = {'0', '1', '2', '3', '\0', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    const string binaryKey(binaryKeyBytes, sizeof(binaryKeyBytes));
    const string modeSwitchID = "binary-key-mode-switch";
    auto modeSwitchMMKV = MMKV::mmkvWithID(modeSwitchID, MMKV_SINGLE_PROCESS, &binaryKey);
    assert(modeSwitchMMKV);
    modeSwitchMMKV->clearAll();
    assert(modeSwitchMMKV->cryptKey() == binaryKey);
    assert(modeSwitchMMKV->set("secret", "payload"));
    assert(modeSwitchMMKV->reKey(binaryKey, true));
    assert(modeSwitchMMKV->cryptKey() == binaryKey);
    modeSwitchMMKV->close();

    modeSwitchMMKV = MMKV::mmkvWithID(modeSwitchID, MMKV_SINGLE_PROCESS, &binaryKey, nullptr, 0, true);
    assert(modeSwitchMMKV);
    string decryptedValue;
    assert(modeSwitchMMKV->getString("payload", decryptedValue));
    assert(decryptedValue == "secret");

    const auto crcPath = rootDir + "/" + modeSwitchID + ".crc";
    const auto beforeClear = readMetaInfo(crcPath);
    modeSwitchMMKV->clearAll(true);
    const auto afterFirstClear = readMetaInfo(crcPath);
    assert(afterFirstClear.m_actualSize == 0);
    assert(memcmp(beforeClear.m_vector, afterFirstClear.m_vector, AES_IV_LEN) != 0);
    modeSwitchMMKV->clearAll(true);
    const auto afterSecondClear = readMetaInfo(crcPath);
    assert(afterSecondClear.m_actualSize == 0);
    assert(memcmp(afterFirstClear.m_vector, afterSecondClear.m_vector, AES_IV_LEN) != 0);
    modeSwitchMMKV->close();

    auto resetMMKV = MMKV::mmkvWithID("crypt-reset-transition");
    assert(resetMMKV);
    resetMMKV->clearAll();
    resetMMKV->checkReSetCryptKey(&binaryKey, true);
    assert(resetMMKV->cryptKey() == binaryKey);
    resetMMKV->checkReSetCryptKey(nullptr);
    assert(resetMMKV->cryptKey().empty());
    resetMMKV->close();

    printf("test crypto random and wipe: passed\n");
}
#endif

void testLongDirectoryWalk() {
    char baseTemplate[] = "/private/tmp/mmkv-walk-XXXXXX";
    auto base = mkdtemp(baseTemplate);
    assert(base);

    string path(base);
    vector<pair<int, string>> directories;
    auto currentFD = open(base, O_RDONLY | O_DIRECTORY);
    assert(currentFD >= 0);

    constexpr size_t targetPathLength = PATH_MAX - 64;
    while (path.size() < targetPathLength) {
        const auto remainingLength = targetPathLength - path.size();
        if (remainingLength == 1) {
            path.push_back('/');
            break;
        }
        auto nameLength = min<size_t>(100, remainingLength - 1);
        string name(nameLength, static_cast<char>('a' + directories.size() % 26));
        assert(mkdirat(currentFD, name.c_str(), 0700) == 0);
        auto childFD = openat(currentFD, name.c_str(), O_RDONLY | O_DIRECTORY);
        assert(childFD >= 0);
        directories.emplace_back(currentFD, name);
        currentFD = childFD;
        path += "/" + name;
    }
    assert(path.size() == targetPathLength);

    const string filename(100, 'z');
    auto fileFD = openat(currentFD, filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    assert(fileFD >= 0);
    close(fileFD);

    vector<string> files;
    walkInDir(path, WalkFile, [&files](const MMKVPath_t &filePath, WalkType) {
        files.push_back(filePath);
    });
    assert(files.size() == 1);
    assert(files.front() == path + "/" + filename);

    assert(unlinkat(currentFD, filename.c_str(), 0) == 0);
    close(currentFD);
    for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
        assert(unlinkat(it->first, it->second.c_str(), AT_REMOVEDIR) == 0);
        close(it->first);
    }
    assert(rmdir(base) == 0);

    printf("test long directory walk: passed\n");
}

void testRemove(MMKV *mmkv) {
    auto ret = mmkv->set(true, "bool_1");
    ret &= mmkv->set(numeric_limits<int32_t>::max(), "int_1");
    ret &= mmkv->set(numeric_limits<int64_t>::max(), "long_1");
    ret &= mmkv->set(numeric_limits<float>::min(), "float_1");
    ret &= mmkv->set(numeric_limits<double>::min(), "double_1");
    ret &= mmkv->set("hello", "string_1");
    vector<string> v = vector<string>{"key", "value"};
    ret &= mmkv->set(v, "vector_1");
    assert(ret);

    {
        long count = mmkv->count();

        mmkv->removeValueForKey("bool_1");
        mmkv->removeValuesForKeys({"int_1", "long_1"});

        auto newCount = mmkv->count();
        assert(count == newCount + 3);
    }

    auto bValue = mmkv->getBool("bool_1");
    assert(!bValue);

    auto iValue = mmkv->getInt32("int_1");
    assert(iValue == 0);

    auto lValue = mmkv->getInt64("long_1");
    assert(lValue == 0);

    auto fValue = mmkv->getFloat("float_1");
    assert(EqualWithAccuracy(fValue, numeric_limits<float>::min(), 0.001f));

    double dValue = mmkv->getDouble("double_1");
    assert(EqualWithAccuracy(dValue, numeric_limits<double>::min(), 0.001));

    string sValue;
    ret = mmkv->getString("string_1", sValue);
    assert(ret && sValue == "hello");

    vector<string> vValue;
    ret = mmkv->getVector("vector_1", vValue);
    assert(ret && vValue == v);

    printf("test remove: passed\n");
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));
    char c;
    srand((uint64_t) &c);

    string rootDir = "/tmp/mmkv";
    MMKV::initializeMMKV(rootDir);

    auto mmkv = MMKV::mmkvWithID("unit_test");
    mmkv->clearAll();

    testBool(mmkv);
    testInt32(mmkv);
    testInt64(mmkv);
    testUInt32(mmkv);
    testUInt64(mmkv);
    testFloat(mmkv);
    testDouble(mmkv);
    testString(mmkv);
    testBytes(mmkv);
    testVector(mmkv);
    testRemove(mmkv);
    testOversizedKey(mmkv);
    testTransactionalOutputAndOversizedValue(mmkv);
    testExpirationOverflow();
#ifndef MMKV_DISABLE_CRYPT
    testCryptoRandomAndWipe(rootDir);
#endif
    testLongDirectoryWalk();
}
