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
#include "crc32/Checksum.h"
#include "MemoryFile.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <limits>
#include <numeric>
#include <new>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace mmkv;

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

void testOversizedValue(MMKV *mmkv) {
    uint8_t sentinel = 0;
    MMBuffer boundary(&sentinel,
                      static_cast<size_t>(numeric_limits<uint32_t>::max()) - 5,
                      MMBufferNoCopy);
    string value;

    mmkv->clearAll();
    assert(mmkv->set("before", "value"));
    assert(!mmkv->set(boundary, "value"));
    assert(mmkv->getString("value", value) && value == "before");
    assert(mmkv->set("after", "value"));
    assert(mmkv->getString("value", value) && value == "after");

    if (numeric_limits<size_t>::max() > numeric_limits<uint32_t>::max()) {
        MMBuffer oversized(&sentinel, static_cast<size_t>(numeric_limits<uint32_t>::max()) + 1, MMBufferNoCopy);

        mmkv->clearAll();
        assert(!mmkv->set(oversized, "oversized-append"));
        assert(mmkv->set("after", "value"));
        assert(mmkv->getString("value", value) && value == "after");

        mmkv->clearAll();
        assert(mmkv->set("before", "value"));
        assert(!mmkv->set(oversized, "value"));
        assert(mmkv->getString("value", value) && value == "before");
        assert(mmkv->set("after", "value"));
        assert(mmkv->getString("value", value) && value == "after");

        auto expiring = MMKV::mmkvWithID("oversized_expiring_value_test");
        expiring->clearAll();
        assert(expiring->enableAutoKeyExpire());
        assert(!expiring->set(oversized, "value", 60));
        assert(expiring->set("after", "value", 60));
        assert(expiring->getString("value", value) && value == "after");
        expiring->clearAll();
    }

    printf("test oversized value: passed\n");
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

void testCodedOutputBounds() {
    array<uint8_t, 16> storage;

    storage.fill(0xA5);
    CodedOutputData fixedOutput(storage.data(), 3);
    bool rejected = false;
    try {
        fixedOutput.writeRawLittleEndian32(0x12345678);
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && fixedOutput.getPosition() == 0 && storage[0] == 0xA5);

    storage.fill(0xA5);
    CodedOutputData shortVarint(storage.data(), 1);
    rejected = false;
    try {
        shortVarint.writeUInt32(128);
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && shortVarint.getPosition() == 0 && storage[0] == 0xA5);

    constexpr array<uint32_t, 6> values32 = {0, 127, 128, 16383, 16384, numeric_limits<uint32_t>::max()};
    const vector<vector<uint8_t>> expected32 = {
        {0x00}, {0x7f}, {0x80, 0x01}, {0xff, 0x7f}, {0x80, 0x80, 0x01}, {0xff, 0xff, 0xff, 0xff, 0x0f},
    };
    for (size_t index = 0; index < values32.size(); index++) {
        storage.fill(0);
        CodedOutputData output(storage.data(), storage.size());
        output.writeUInt32(values32[index]);
        assert(output.getPosition() == expected32[index].size());
        assert(equal(expected32[index].begin(), expected32[index].end(), storage.begin()));
    }

    constexpr array<uint64_t, 6> values64 = {0, 127, 128, 16383, 16384, numeric_limits<uint64_t>::max()};
    const vector<vector<uint8_t>> expected64 = {
        {0x00},
        {0x7f},
        {0x80, 0x01},
        {0xff, 0x7f},
        {0x80, 0x80, 0x01},
        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01},
    };
    for (size_t index = 0; index < values64.size(); index++) {
        storage.fill(0);
        CodedOutputData output(storage.data(), storage.size());
        output.writeUInt64(values64[index]);
        assert(output.getPosition() == expected64[index].size());
        assert(equal(expected64[index].begin(), expected64[index].end(), storage.begin()));
    }

    storage.fill(0);
    CodedOutputData littleEndianOutput(storage.data(), sizeof(uint32_t));
    littleEndianOutput.writeRawLittleEndian32(0x12345678);
    constexpr array<uint8_t, 4> littleEndian32 = {0x78, 0x56, 0x34, 0x12};
    assert(equal(littleEndian32.begin(), littleEndian32.end(), storage.begin()));

    const char payload[] = "data";
    storage.fill(0xA5);
    CodedOutputData dataOutput(storage.data(), 4);
    rejected = false;
    try {
        dataOutput.writeData(MMBuffer((void *) payload, 4, MMBufferNoCopy));
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && dataOutput.getPosition() == 0 && all_of(storage.begin(), storage.end(), [](auto byte) {
               return byte == 0xA5;
           }));

    storage.fill(0xA5);
    CodedOutputData stringOutput(storage.data(), 4);
    rejected = false;
    try {
        stringOutput.writeString("data");
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && stringOutput.getPosition() == 0 && all_of(storage.begin(), storage.end(), [](auto byte) {
               return byte == 0xA5;
           }));

    CodedOutputData positionOutput(storage.data(), 4);
    positionOutput.setPosition(2);
    rejected = false;
    try {
        positionOutput.seek(3);
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && positionOutput.getPosition() == 2);
    rejected = false;
    try {
        positionOutput.setPosition(5);
    } catch (const out_of_range &) {
        rejected = true;
    }
    assert(rejected && positionOutput.getPosition() == 2);

    if (numeric_limits<size_t>::max() > numeric_limits<uint32_t>::max()) {
        storage.fill(0xA5);
        MMBuffer oversized(storage.data(), static_cast<size_t>(numeric_limits<uint32_t>::max()) + 1,
                           MMBufferNoCopy);
        CodedOutputData output(storage.data(), storage.size());
        rejected = false;
        try {
            output.writeData(oversized);
        } catch (const length_error &) {
            rejected = true;
        }
        assert(rejected && output.getPosition() == 0 && storage[0] == 0xA5);
    }

    printf("test coded output bounds: passed\n");
}

void testExpirationAlignment() {
    auto mmkv = MMKV::mmkvWithID("expiration_alignment_test");
    mmkv->clearAll();
    assert(mmkv->enableAutoKeyExpire());
    for (size_t length = 1; length <= 4; length++) {
        auto key = "alignment-" + to_string(length);
        assert(mmkv->set(string(length, 'x'), key, 60));
        string value;
        assert(mmkv->getString(key, value) && value == string(length, 'x'));
    }
    assert(mmkv->count(true) == 4);
    mmkv->clearAll();

#ifndef MMKV_DISABLE_CRYPT
    const string cryptKey = "alignment-key";
    auto encrypted = MMKV::mmkvWithID("expiration_alignment_crypt_test", MMKV_SINGLE_PROCESS, &cryptKey);
    encrypted->clearAll();
    assert(encrypted->enableAutoKeyExpire());
    for (size_t length = 1; length <= 4; length++) {
        auto key = "alignment-" + to_string(length);
        assert(encrypted->set(string(length, 'x'), key, 60));
        string value;
        assert(encrypted->getString(key, value) && value == string(length, 'x'));
    }
    assert(encrypted->count(true) == 4);
    encrypted->clearAll();
#endif

    printf("test expiration alignment: passed\n");
}

void testArmCRC32() {
#if defined(MMKV_USE_ARMV8_CRC32) && !defined(MMKV_OHOS)
    if (CRC32 != mmkv::armv8_crc32) {
        return;
    }
    array<uint8_t, 265> storage = {};
    for (size_t index = 0; index < storage.size(); index++) {
        storage[index] = static_cast<uint8_t>(index * 37 + 11);
    }
    constexpr array<uint32_t, 3> seeds = {0, 1, 0xdeadbeef};
    for (auto seed : seeds) {
        for (size_t offset = 0; offset < 8; offset++) {
            for (size_t length = 0; length <= 257; length++) {
                auto ptr = storage.data() + offset;
                assert(mmkv::armv8_crc32(seed, ptr, length) == ZLIB_CRC32(seed, ptr, length));
            }
        }
    }
    printf("test ARM CRC32 alignment: passed\n");
#endif
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

void testCryptoRandomAndWipe() {
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

    const string exactAES128Key = "0123456789abcdef";
    auto encrypted = MMKV::mmkvWithID("aes-mode-switch", MMKV_SINGLE_PROCESS, &exactAES128Key);
    encrypted->clearAll();
    assert(encrypted->cryptKey() == exactAES128Key);
    assert(encrypted->set("payload", "value"));
    assert(encrypted->reKey(exactAES128Key, true));
    string value;
    assert(encrypted->getString("value", value) && value == "payload");
    encrypted->close();

    encrypted = MMKV::mmkvWithID("aes-mode-switch", MMKV_SINGLE_PROCESS, &exactAES128Key, nullptr, 0, true);
    assert(encrypted->getString("value", value) && value == "payload");
    encrypted->clearAll();
    encrypted->close();

    const string transitionKey = "transition-key";
    auto transition = MMKV::mmkvWithID("aes-reset-transition");
    transition->clearAll();
    transition->checkReSetCryptKey(&transitionKey, false);
    assert(transition->set("encrypted", "value"));
    assert(transition->getString("value", value) && value == "encrypted");
    transition->clearAll();
    transition->checkReSetCryptKey(nullptr);
    assert(transition->set("plain", "value"));
    assert(transition->getString("value", value) && value == "plain");
    transition->clearAll();
    transition->checkReSetCryptKey(&transitionKey, true);
    assert(transition->set("aes256", "value"));
    assert(transition->getString("value", value) && value == "aes256");
    transition->clearAll();
    transition->close();

    printf("test crypto random and wipe: passed\n");
}
#endif

void testLongDirectoryWalk(const string &testRoot) {
    auto baseTemplate = testRoot + "/mmkv-walk-XXXXXX";
    vector<char> mutableTemplate(baseTemplate.begin(), baseTemplate.end());
    mutableTemplate.push_back('\0');
    auto base = mkdtemp(mutableTemplate.data());
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

void testMinimalBackupRestore(const string &rootDir) {
    const string suffixID = "suffix.crc";
    const string specialID = "special/id";
    auto suffix = MMKV::mmkvWithID(suffixID);
    auto special = MMKV::mmkvWithID(specialID);
    suffix->clearAll();
    special->clearAll();
    assert(suffix->set("before", "value"));
    assert(special->set("before-special", "value"));

    const auto suffixPath = rootDir + "/" + suffixID;
    const auto suffixCRCPath = suffixPath + ".crc";
    struct stat beforeAlias = {};
    struct stat afterAlias = {};
    struct stat beforeCRCAlias = {};
    struct stat afterCRCAlias = {};
    assert(stat(suffixPath.c_str(), &beforeAlias) == 0);
    assert(stat(suffixCRCPath.c_str(), &beforeCRCAlias) == 0);
    const auto rootAlias = rootDir + "/.";
    assert(MMKV::backupOneToDirectory(suffixID, rootAlias, &rootDir));
    assert(MMKV::restoreOneFromDirectory(suffixID, rootAlias, &rootDir));
    assert(MMKV::backupAllToDirectory(rootAlias, &rootDir) != 0);
    assert(MMKV::restoreAllFromDirectory(rootAlias, &rootDir) != 0);
    assert(stat(suffixPath.c_str(), &afterAlias) == 0);
    assert(stat(suffixCRCPath.c_str(), &afterCRCAlias) == 0);
    assert(beforeAlias.st_dev == afterAlias.st_dev && beforeAlias.st_ino == afterAlias.st_ino);
    assert(beforeCRCAlias.st_dev == afterCRCAlias.st_dev && beforeCRCAlias.st_ino == afterCRCAlias.st_ino);
    string value;
    assert(suffix->getString("value", value) && value == "before");

    const auto uniqueSuffix = to_string(static_cast<unsigned long long>(getpid()));
    const auto backupDir = rootDir + "-backup-" + uniqueSuffix;
    assert(MMKV::backupAllToDirectory(backupDir, &rootDir) >= 2);

    assert(suffix->set("after", "value"));
    assert(special->set("after-special", "value"));
    assert(MMKV::restoreAllFromDirectory(backupDir, &rootDir) >= 2);
    assert(suffix->getString("value", value) && value == "before");
    assert(special->getString("value", value) && value == "before-special");

    const auto invalidDir = rootDir + "-invalid-" + uniqueSuffix;
    assert(mkPath(invalidDir));
    assert(copyFile(backupDir + "/" + suffixID, invalidDir + "/" + suffixID));
    auto invalidCRC = invalidDir + "/" + suffixID + ".crc";
    auto invalidFD = open(invalidCRC.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    assert(invalidFD >= 0);
    const uint8_t truncatedMetadata = 0;
    assert(write(invalidFD, &truncatedMetadata, sizeof(truncatedMetadata)) ==
           static_cast<ssize_t>(sizeof(truncatedMetadata)));
    close(invalidFD);

    assert(suffix->set("unchanged", "value"));
    assert(!MMKV::restoreOneFromDirectory(suffixID, invalidDir, &rootDir));
    assert(suffix->getString("value", value) && value == "unchanged");

    assert(copyFile(backupDir + "/" + suffixID + ".crc", invalidCRC));
    invalidFD = open((invalidDir + "/" + suffixID).c_str(), O_WRONLY | O_TRUNC);
    assert(invalidFD >= 0);
    close(invalidFD);
    assert(!MMKV::restoreOneFromDirectory(suffixID, invalidDir, &rootDir));
    assert(suffix->getString("value", value) && value == "unchanged");

    suffix->clearAll();
    special->clearAll();
    std::filesystem::remove_all(backupDir);
    std::filesystem::remove_all(invalidDir);
    printf("test minimal backup and restore hardening: passed\n");
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

int main(int argc, char *argv[]) {
    locale::global(locale(""));
    wcout.imbue(locale(""));
    char c;
    srand((uint64_t) &c);

    string rootDir = argc > 1 ? argv[1] : "/tmp/mmkv";
    assert(mkPath(rootDir));
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
    testOversizedValue(mmkv);
    testCodedOutputBounds();
    testExpirationOverflow();
    testExpirationAlignment();
    testArmCRC32();
#ifndef MMKV_DISABLE_CRYPT
    testCryptoRandomAndWipe();
#endif
    testLongDirectoryWalk(rootDir);
    testMinimalBackupRestore(rootDir);
}
