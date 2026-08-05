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

void testLongDirectoryWalk(const string &testRoot) {
    string baseTemplate = testRoot + "/mmkv-walk-XXXXXX";
    vector<char> mutableTemplate(baseTemplate.begin(), baseTemplate.end());
    mutableTemplate.push_back('\0');
    auto base = mkdtemp(mutableTemplate.data());
    assert(base);

    string path(base);
    vector<pair<int, string>> directories;
    auto currentFD = open(base, O_RDONLY | O_DIRECTORY);
    assert(currentFD >= 0);

    const size_t targetPathLength = max(path.size(), static_cast<size_t>(PATH_MAX - 64));
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
    constexpr char fileContent[] = "descriptor-relative copy";
    assert(write(fileFD, fileContent, sizeof(fileContent)) == sizeof(fileContent));
    close(fileFD);

    const string symlinkName = "symlink";
    const string fifoName = "fifo";
    const string folderName = "folder";
    assert(symlinkat(filename.c_str(), currentFD, symlinkName.c_str()) == 0);
    assert(mkfifoat(currentFD, fifoName.c_str(), 0600) == 0);
    assert(mkdirat(currentFD, folderName.c_str(), 0700) == 0);

    vector<string> files;
    walkInDir(path, WalkFile, [&files](const MMKVPath_t &filePath, WalkType) {
        files.push_back(filePath);
    });
    assert(files.size() == 1);
    assert(files.front() == path + "/" + filename);

    vector<string> folders;
    walkInDir(path, WalkFolder, [&folders](const MMKVPath_t &folderPath, WalkType) {
        folders.push_back(folderPath);
    });
    assert(folders.size() == 1);
    assert(folders.front() == path + "/" + folderName);

    auto pinnedFile = openRegularFileInDir(path, filename);
    assert(pinnedFile != MMKVFileHandleInvalidValue);
    assert(openRegularFileInDir(path, symlinkName) == MMKVFileHandleInvalidValue);
    assert(openRegularFileInDir(path, fifoName) == MMKVFileHandleInvalidValue);

    const string copyName = "copy";
    auto copyFD = openat(currentFD, copyName.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0600);
    assert(copyFD >= 0);
    assert(copyFileContent(pinnedFile, copyFD));
    assert(lseek(copyFD, 0, SEEK_SET) == 0);
    char copiedContent[sizeof(fileContent)] = {};
    assert(read(copyFD, copiedContent, sizeof(copiedContent)) == sizeof(copiedContent));
    assert(memcmp(copiedContent, fileContent, sizeof(fileContent)) == 0);
    close(copyFD);
    closeFileHandle(pinnedFile);

    assert(unlinkat(currentFD, filename.c_str(), 0) == 0);
    assert(unlinkat(currentFD, copyName.c_str(), 0) == 0);
    assert(unlinkat(currentFD, symlinkName.c_str(), 0) == 0);
    assert(unlinkat(currentFD, fifoName.c_str(), 0) == 0);
    assert(unlinkat(currentFD, folderName.c_str(), AT_REMOVEDIR) == 0);
    close(currentFD);
    for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
        assert(unlinkat(it->first, it->second.c_str(), AT_REMOVEDIR) == 0);
        close(it->first);
    }
    assert(rmdir(base) == 0);

    printf("test long directory walk: passed\n");
}

static void writeTestFile(const string &path, const char *content) {
    auto fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    assert(fd >= 0);
    const auto size = strlen(content);
    assert(write(fd, content, size) == static_cast<ssize_t>(size));
    close(fd);
}

static void writeTestBytes(const string &path, const void *content, size_t size) {
    auto fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    assert(fd >= 0);
    size_t offset = 0;
    while (offset < size) {
        auto count = write(fd, static_cast<const uint8_t *>(content) + offset, size - offset);
        assert(count > 0);
        offset += static_cast<size_t>(count);
    }
    close(fd);
}

class OneShotLogHandler final : public MMKVHandler {
    string m_function;
    string m_messageFragment;
    function<void()> m_action;

public:
    bool fired = false;

    OneShotLogHandler(string functionName, string messageFragment, function<void()> action)
        : m_function(std::move(functionName))
        , m_messageFragment(std::move(messageFragment))
        , m_action(std::move(action)) {}

    void mmkvLog(MMKVLogLevel, const char *, int, const char *functionName, MMKVLog_t message) override {
        if (!fired && functionName && m_function == functionName && message.find(m_messageFragment) != string::npos) {
            fired = true;
            m_action();
        }
    }
};

static string readTestFile(const string &path) {
    ifstream input(path, ios::binary);
    assert(input.is_open());
    return {istreambuf_iterator<char>(input), istreambuf_iterator<char>()};
}

void testMayflyMappedFileIdentity(const string &testRoot) {
    string baseTemplate = testRoot + "/mmkv-mapped-identity-XXXXXX";
    vector<char> mutableTemplate(baseTemplate.begin(), baseTemplate.end());
    mutableTemplate.push_back('\0');
    auto base = mkdtemp(mutableTemplate.data());
    assert(base);

    const string livePath = string(base) + "/live";
    const string pinnedPath = string(base) + "/live-pinned";
    const auto pageSize = getPageSize();
    {
        MemoryFile mapped(livePath, pageSize, false, true);
        assert(mapped.isFileValid());
        auto pinnedFD = open(livePath.c_str(), O_RDWR);
        assert(pinnedFD >= 0);
        assert(mapped.isMappedFile(pinnedFD));

        // The multiprocess size-change path passes MemoryFile's own mayfly fd.
        // Cleanup closes that descriptor, so reload must rely on duplicates
        // captured before cleanup for both validation and the retained handle.
        auto ownMayflyFD = mapped.getFd();
        assert(ownMayflyFD != MMKVFileHandleInvalidValue);
        assert(mapped.reloadFromFileHandle(ownMayflyFD, pageSize));
        assert(mapped.isMappedFile(pinnedFD));
        assert(isSameFile(mapped.getFd(), pinnedFD));
        mapped.cleanMayflyFD(true);

        assert(rename(livePath.c_str(), pinnedPath.c_str()) == 0);
        auto aliasFD = open((string(base) + "/./live-pinned").c_str(), O_RDWR);
        assert(aliasFD >= 0);
        assert(mapped.isMappedFile(aliasFD));

        writeTestFile(livePath, "replacement-must-not-change");
        auto replacementFD = open(livePath.c_str(), O_RDWR);
        assert(replacementFD >= 0);
        struct stat replacementBefore = {};
        assert(fstat(replacementFD, &replacementBefore) == 0);
        assert(!mapped.isMappedFile(replacementFD));

        // All operations that need to recreate a mayfly descriptor must fail
        // closed while a mapping still belongs to the renamed inode.
        assert(mapped.getFd() == MMKVFileHandleInvalidValue);
        assert(mapped.getActualFileSize() == pageSize);
        assert(!mapped.truncate(pageSize * 2));
        struct stat replacementAfter = {};
        assert(fstat(replacementFD, &replacementAfter) == 0);
        assert(replacementAfter.st_size == replacementBefore.st_size);
        assert(mapped.isMappedFile(pinnedFD));

        // A caller that already pinned the intended inode can deliberately
        // clear and reload without resolving the replaced path.
        mapped.clearMemoryCache();
        assert(mapped.reloadFromFileHandle(aliasFD, pageSize));
        assert(mapped.isMappedFile(pinnedFD));
        assert(isSameFile(mapped.getFd(), pinnedFD));
        assert(mapped.getActualFileSize() == pageSize);
        mapped.cleanMayflyFD(true);
        assert(mapped.getFd() == MMKVFileHandleInvalidValue);

        close(replacementFD);
        close(aliasFD);
        close(pinnedFD);
    }

    // Disk-error recovery intentionally deletes and recreates a corrupt file.
    // Its explicit identity reset must permit the new inode while ordinary
    // lazy cache clears continue to reject replacements.
    const string resetPath = string(base) + "/intentional-reset";
    {
        MemoryFile resetFile(resetPath, pageSize, false, true);
        assert(resetFile.isFileValid());
        auto oldFD = open(resetPath.c_str(), O_RDWR);
        assert(oldFD >= 0 && resetFile.isMappedFile(oldFD));
        resetFile.clearMemoryCache(true);
        assert(unlink(resetPath.c_str()) == 0);
        resetFile.reloadFromFile(pageSize);
        assert(resetFile.isFileValid());
        assert(!resetFile.isMappedFile(oldFD));
        close(oldFD);
    }

    error_code cleanupError;
    fs::remove_all(base, cleanupError);
    assert(!cleanupError);
    printf("test mayfly mapped-file identity: passed\n");
}

void testPinnedDirectoryHandles(const string &testRoot) {
    string baseTemplate = testRoot + "/mmkv-pinned-XXXXXX";
    vector<char> mutableTemplate(baseTemplate.begin(), baseTemplate.end());
    mutableTemplate.push_back('\0');
    auto base = mkdtemp(mutableTemplate.data());
    assert(base);

    const string sourceDir = string(base) + "/source";
    const string pinnedDir = string(base) + "/source-pinned";
    const string destinationDir = string(base) + "/destination";
    const string outsidePath = string(base) + "/outside";
    error_code createError;
    fs::create_directories(sourceDir, createError);
    assert(!createError);
    fs::create_directories(destinationDir, createError);
    assert(!createError);

    constexpr char originalData[] = "original-data";
    writeTestFile(sourceDir + "/good", originalData);
    writeTestFile(sourceDir + "/good.crc", "original-metadata");
    writeTestFile(sourceDir + "/short", "x");
    fs::create_directories(sourceDir + "/nested", createError);
    assert(!createError);
    writeTestFile(sourceDir + "/nested/good", originalData);
    writeTestFile(sourceDir + "/nested/good.crc", "original-metadata");
    assert(symlink("good", (sourceDir + "/link").c_str()) == 0);
    assert(symlink("good.crc", (sourceDir + "/link.crc").c_str()) == 0);
    assert(mkfifo((sourceDir + "/fifo").c_str(), 0600) == 0);
    assert(mkdir((sourceDir + "/folder").c_str(), 0700) == 0);

    auto sourceDirFD = openDirectoryHandle(sourceDir);
    assert(sourceDirFD != MMKVFileHandleInvalidValue);
    assert(rename(sourceDir.c_str(), pinnedDir.c_str()) == 0);
    fs::create_directories(sourceDir, createError);
    assert(!createError);
    writeTestFile(sourceDir + "/good", "replacement-data");
    writeTestFile(sourceDir + "/good.crc", "replacement-metadata");
    fs::create_directories(sourceDir + "/nested", createError);
    assert(!createError);
    writeTestFile(sourceDir + "/nested/good", "replacement-data");
    writeTestFile(sourceDir + "/nested/good.crc", "replacement-metadata");

    vector<string> files;
    assert(walkInOpenedDir(sourceDirFD, sourceDir, WalkFile, [&](const MMKVPath_t &fileName, WalkType) {
        files.push_back(fileName);
    }));
    sort(files.begin(), files.end());
    assert((files == vector<string>{"good", "good.crc", "short"}));

    MMKVFileHandle_t dataFD = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t crcFD = MMKVFileHandleInvalidValue;
    assert(openRegularFilePairInDir(sourceDirFD, sourceDir, "good", "good.crc", dataFD, crcFD));
    string pinnedData(strlen(originalData), '\0');
    assert(readFileContent(dataFD, pinnedData.data(), pinnedData.size()));
    assert(pinnedData == originalData);

    auto nestedDirFD = openDirectoryInDir(sourceDirFD, sourceDir, "nested", false);
    assert(nestedDirFD != MMKVFileHandleInvalidValue);
    MMKVFileHandle_t nestedDataFD = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t nestedCRCFD = MMKVFileHandleInvalidValue;
    assert(openRegularFilePairInDir(nestedDirFD, sourceDir + "/nested", "good", "good.crc", nestedDataFD,
                                    nestedCRCFD));
    string nestedPinnedData(strlen(originalData), '\0');
    assert(readFileContent(nestedDataFD, nestedPinnedData.data(), nestedPinnedData.size()));
    assert(nestedPinnedData == originalData);
    closeFileHandle(nestedDataFD);
    closeFileHandle(nestedCRCFD);
    closeFileHandle(nestedDirFD);

    MMKVFileHandle_t unsafeDataFD = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t unsafeCRCFD = MMKVFileHandleInvalidValue;
    assert(!openRegularFilePairInDir(sourceDirFD, sourceDir, "link", "link.crc", unsafeDataFD, unsafeCRCFD));
    assert(unsafeDataFD == MMKVFileHandleInvalidValue && unsafeCRCFD == MMKVFileHandleInvalidValue);
    const string embeddedNull("good\0suffix", 11);
    assert(openRegularFileInDir(sourceDirFD, sourceDir, embeddedNull) == MMKVFileHandleInvalidValue);
    const string embeddedNullDir = destinationDir + string("\0ignored", 8);
    assert(openDirectoryHandle(embeddedNullDir) == MMKVFileHandleInvalidValue);
    assert(openOrCreateDirectoryHandle(embeddedNullDir) == MMKVFileHandleInvalidValue);
    assert(!mkPath(embeddedNullDir));
    const string invalidPrefix = string(base) + "/invalid-prefix";
    assert(openOrCreateDirectoryHandle(invalidPrefix + "/created-before-parent/../escape") ==
           MMKVFileHandleInvalidValue);
    assert(!fs::exists(invalidPrefix));

    auto shortFD = openRegularFileInDir(sourceDirFD, sourceDir, "short");
    assert(shortFD != MMKVFileHandleInvalidValue);
    const array<uint8_t, 4> sentinel = {0x11, 0x22, 0x33, 0x44};
    auto unchanged = sentinel;
    assert(!readFileContent(shortFD, unchanged.data(), unchanged.size()));
    assert(unchanged == sentinel);
    closeFileHandle(shortFD);

    auto destinationDirFD = openDirectoryHandle(destinationDir);
    assert(destinationDirFD != MMKVFileHandleInvalidValue);
    writeTestFile(outsidePath, "outside-must-not-change");
    assert(symlink("../outside", (destinationDir + "/link").c_str()) == 0);
    assert(openOrCreateRegularFileInDir(destinationDirFD, destinationDir, "link") ==
           MMKVFileHandleInvalidValue);
    assert(!copyFileContent(dataFD, destinationDirFD, destinationDir, "link"));
    assert(readTestFile(outsidePath) == "outside-must-not-change");

    writeTestFile(destinationDir + "/existing", "old-content-with-a-long-tail");
    assert(copyFileContent(dataFD, destinationDirFD, destinationDir, "existing"));
    assert(readTestFile(destinationDir + "/existing") == originalData);
    assert(copyFileContent(dataFD, destinationDirFD, destinationDir, "created"));
    assert(readTestFile(destinationDir + "/created") == originalData);
    struct stat createdInfo = {};
    assert(stat((destinationDir + "/created").c_str(), &createdInfo) == 0);
    assert((createdInfo.st_mode & 0777) == 0600);

    assert(copyFile(dataFD, destinationDirFD, destinationDir, "atomic"));
    assert(readTestFile(destinationDir + "/atomic") == originalData);

    const string outsideDirectory = string(base) + "/outside-directory";
    const string redirectedDirectory = string(base) + "/redirected-directory";
    fs::create_directories(outsideDirectory, createError);
    assert(!createError);
    assert(symlink(outsideDirectory.c_str(), redirectedDirectory.c_str()) == 0);
    auto unsafeCreatedDir = openOrCreateDirectoryHandle(redirectedDirectory + "/must-not-exist/nested");
    assert(unsafeCreatedDir == MMKVFileHandleInvalidValue);
    assert(!fs::exists(outsideDirectory + "/must-not-exist"));

    closeFileHandle(crcFD);
    closeFileHandle(dataFD);
    closeFileHandle(destinationDirFD);
    closeFileHandle(sourceDirFD);
    error_code cleanupError;
    fs::remove_all(base, cleanupError);
    assert(!cleanupError);
    printf("test pinned directory handles: passed\n");
}

void testPairFailureRollback(const string &testRoot) {
    string baseTemplate = testRoot + "/mmkv-pair-rollback-XXXXXX";
    vector<char> mutableTemplate(baseTemplate.begin(), baseTemplate.end());
    mutableTemplate.push_back('\0');
    auto base = mkdtemp(mutableTemplate.data());
    assert(base);

    const string sourceDir = string(base) + "/source";
    const string destinationDir = string(base) + "/destination";
    error_code createError;
    fs::create_directories(sourceDir, createError);
    assert(!createError);
    fs::create_directories(destinationDir, createError);
    assert(!createError);
    writeTestFile(sourceDir + "/data", "new-data");
    writeTestFile(sourceDir + "/data.crc", "new-crc");
    writeTestFile(destinationDir + "/data", "old-data");
    writeTestFile(destinationDir + "/data.crc", "old-crc");

    auto sourceData = open((sourceDir + "/data").c_str(), O_RDONLY);
    auto sourceCRC = open((sourceDir + "/data.crc").c_str(), O_RDONLY);
    auto destinationData = open((destinationDir + "/data").c_str(), O_RDWR);
    // A read-only second destination deterministically fails only after the
    // first destination has been overwritten. Its original bytes remain
    // readable, while the pair helper must roll the first file back.
    auto destinationCRC = open((destinationDir + "/data.crc").c_str(), O_RDONLY);
    auto destinationDirFD = openDirectoryHandle(destinationDir);
    assert(sourceData >= 0 && sourceCRC >= 0 && destinationData >= 0 && destinationCRC >= 0);
    assert(destinationDirFD != MMKVFileHandleInvalidValue);
    assert(!copyFileContentPair(sourceData, sourceCRC, destinationData, destinationCRC, destinationDirFD,
                                destinationDir));
    assert(readTestFile(destinationDir + "/data") == "old-data");
    assert(readTestFile(destinationDir + "/data.crc") == "old-crc");
    close(sourceData);
    close(sourceCRC);
    close(destinationData);
    close(destinationCRC);

    // Exercise the restore-style transaction's late second-destination
    // failure with destinations that did not both exist before the call.
    // POSIX cannot atomically unlink by open-file identity. Failed transactions
    // leave their empty created entries instead of risking deletion of a
    // concurrent replacement through a check-then-unlink race.
    auto firstSource = open((sourceDir + "/data").c_str(), O_RDONLY);
    auto secondSource = open((sourceDir + "/data.crc").c_str(), O_RDONLY);
    assert(firstSource >= 0 && secondSource >= 0);

    MMKVFileHandle_t freshData = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t freshCRC = MMKVFileHandleInvalidValue;
    bool freshDataCreated = false;
    bool freshCRCCreated = false;
    assert(openOrCreateRegularFilePairInDir(destinationDirFD, destinationDir, "fresh", "fresh.crc", freshData,
                                            freshCRC, freshDataCreated, freshCRCCreated));
    assert(freshDataCreated && freshCRCCreated);
    closeFileHandle(freshCRC);
    freshCRC = open((destinationDir + "/fresh.crc").c_str(), O_RDONLY);
    assert(freshCRC >= 0);
    assert(!copyFileContentPair(firstSource, secondSource, freshData, freshCRC, destinationDirFD, destinationDir,
                                "fresh", "fresh.crc", freshDataCreated, freshCRCCreated));
    closeFileHandle(freshData);
    closeFileHandle(freshCRC);
    assert(readTestFile(destinationDir + "/fresh").empty());
    assert(readTestFile(destinationDir + "/fresh.crc").empty());

    writeTestFile(destinationDir + "/one-sided", "keep-me");
    MMKVFileHandle_t oneSidedData = MMKVFileHandleInvalidValue;
    MMKVFileHandle_t oneSidedCRC = MMKVFileHandleInvalidValue;
    bool oneSidedDataCreated = false;
    bool oneSidedCRCCreated = false;
    assert(openOrCreateRegularFilePairInDir(destinationDirFD, destinationDir, "one-sided", "one-sided.crc",
                                            oneSidedData, oneSidedCRC, oneSidedDataCreated,
                                            oneSidedCRCCreated));
    assert(!oneSidedDataCreated && oneSidedCRCCreated);
    closeFileHandle(oneSidedCRC);
    oneSidedCRC = open((destinationDir + "/one-sided.crc").c_str(), O_RDONLY);
    assert(oneSidedCRC >= 0);
    assert(!copyFileContentPair(firstSource, secondSource, oneSidedData, oneSidedCRC, destinationDirFD,
                                destinationDir, "one-sided", "one-sided.crc", oneSidedDataCreated,
                                oneSidedCRCCreated));
    closeFileHandle(oneSidedData);
    closeFileHandle(oneSidedCRC);
    assert(readTestFile(destinationDir + "/one-sided") == "keep-me");
    assert(readTestFile(destinationDir + "/one-sided.crc").empty());

    // Force copyFilePair() to fail its second commit while an initially absent
    // first entry is concurrently created. The no-replace publish must let the
    // newcomer win without deleting or overwriting it during rollback.
    const string largeSourcePath = sourceDir + "/large-crc";
    auto largeSource = open(largeSourcePath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0600);
    assert(largeSource >= 0);
    constexpr off_t raceWindowSize = 64 * 1024 * 1024;
    assert(ftruncate(largeSource, raceWindowSize) == 0);

    atomic<bool> scannerReady{false};
    atomic<bool> newcomerPairFinished{false};
    bool newcomerInstalled = false;
    thread newcomer([&] {
        auto scanFD = dup(destinationDirFD);
        assert(scanFD >= 0);
        auto directory = fdopendir(scanFD);
        assert(directory);
        scannerReady.store(true, memory_order_release);
        while (!newcomerPairFinished.load(memory_order_acquire)) {
            rewinddir(directory);
            string commitTempName;
            while (auto entry = readdir(directory)) {
                if (strncmp(entry->d_name, ".mmkv.tmp.", strlen(".mmkv.tmp.")) == 0) {
                    commitTempName = entry->d_name;
                    break;
                }
            }
            if (!commitTempName.empty()) {
                // createTemporaryFileInDir() briefly has an empty name before
                // unlinking its staging file. Only the real commit temp gains
                // content while it remains visible.
                struct stat tempInfo = {};
                if (fstatat(destinationDirFD, commitTempName.c_str(), &tempInfo, AT_SYMLINK_NOFOLLOW) != 0 ||
                    !S_ISREG(tempInfo.st_mode) || tempInfo.st_size == 0) {
                    continue;
                }
                auto newcomerFD = openat(destinationDirFD, "newcomer", O_CREAT | O_EXCL | O_WRONLY, 0600);
                assert(newcomerFD >= 0);
                constexpr char newcomerContent[] = "newcomer-wins";
                assert(write(newcomerFD, newcomerContent, sizeof(newcomerContent) - 1) ==
                       static_cast<ssize_t>(sizeof(newcomerContent) - 1));
                close(newcomerFD);
                newcomerInstalled = true;
                closedir(directory);
                return;
            }
            this_thread::yield();
        }
        closedir(directory);
    });
    while (!scannerReady.load(memory_order_acquire)) {
        this_thread::yield();
    }
    auto newcomerResult =
        copyFilePair(largeSource, secondSource, destinationDirFD, destinationDir, "newcomer", "newcomer.crc");
    newcomerPairFinished.store(true, memory_order_release);
    newcomer.join();
    assert(!newcomerResult);
    assert(newcomerInstalled);
    assert(readTestFile(destinationDir + "/newcomer") == "newcomer-wins");
    assert(!fs::exists(destinationDir + "/newcomer.crc"));

    atomic<bool> pairFinished{false};
    bool replacementInstalled = false;
    thread replacer([&] {
        struct stat entryInfo = {};
        while (!pairFinished.load(memory_order_acquire)) {
            if (fstatat(destinationDirFD, "raced", &entryInfo, AT_SYMLINK_NOFOLLOW) == 0) {
                assert(renameat(destinationDirFD, "raced", destinationDirFD, "raced-transaction") == 0);
                auto replacementFD = openat(destinationDirFD, "raced", O_CREAT | O_EXCL | O_WRONLY, 0600);
                assert(replacementFD >= 0);
                constexpr char replacement[] = "concurrent-replacement";
                assert(write(replacementFD, replacement, sizeof(replacement) - 1) ==
                       static_cast<ssize_t>(sizeof(replacement) - 1));
                close(replacementFD);
                assert(mkdirat(destinationDirFD, "raced.crc", 0700) == 0);
                replacementInstalled = true;
                return;
            }
            this_thread::yield();
        }
    });
    auto raceResult = copyFilePair(firstSource, largeSource, destinationDirFD, destinationDir, "raced", "raced.crc");
    pairFinished.store(true, memory_order_release);
    replacer.join();
    assert(!raceResult);
    assert(replacementInstalled);
    assert(readTestFile(destinationDir + "/raced") == "concurrent-replacement");
    assert(fs::is_directory(destinationDir + "/raced.crc"));
    close(largeSource);
    close(firstSource);
    close(secondSource);
    closeFileHandle(destinationDirFD);

    error_code cleanupError;
    fs::remove_all(base, cleanupError);
    assert(!cleanupError);
    printf("test pair failure rollback: passed\n");
}

void testBackupRestoreNoFollow(const string &testRoot) {
    string baseTemplate = testRoot + "/mmkv-backup-XXXXXX";
    vector<char> mutableTemplate(baseTemplate.begin(), baseTemplate.end());
    mutableTemplate.push_back('\0');
    auto base = mkdtemp(mutableTemplate.data());
    assert(base);

    const string sourceDir = string(base) + "/source";
    const string backupDir = string(base) + "/backup";
    const string restoreDir = string(base) + "/restore";
    error_code createError;
    fs::create_directories(sourceDir, createError);
    assert(!createError);

    writeTestFile(sourceDir + "/good", "data");
    MMKVMetaInfo metadata;
    metadata.m_crcDigest = 0x12345678;
    metadata.m_sequence = 7;
    writeTestBytes(sourceDir + "/good.crc", &metadata, sizeof(metadata));
    const string metadataBytes(reinterpret_cast<const char *>(&metadata), sizeof(metadata));
    // CRC_SUFFIX is legal in an mmapID. Its data basename must not be
    // discarded merely because it also looks like another store's metadata.
    writeTestFile(sourceDir + "/suffix.crc", "suffix-data");
    writeTestBytes(sourceDir + "/suffix.crc.crc", &metadata, sizeof(metadata));
    const string outsideHardlinkData = string(base) + "/outside-hardlink-data";
    const string outsideHardlinkCRC = string(base) + "/outside-hardlink-crc";
    writeTestFile(outsideHardlinkData, "outside-hardlink-data");
    writeTestBytes(outsideHardlinkCRC, &metadata, sizeof(metadata));
    assert(link(outsideHardlinkData.c_str(), (sourceDir + "/hard").c_str()) == 0);
    assert(link(outsideHardlinkCRC.c_str(), (sourceDir + "/hard.crc").c_str()) == 0);
    assert(symlink("good", (sourceDir + "/bad").c_str()) == 0);
    assert(symlink("good.crc", (sourceDir + "/bad.crc").c_str()) == 0);

    // Lexically different spellings of the same pinned directory are no-op
    // aliases. In particular, backup must not rename-replace the live pair.
    struct stat aliasDataBefore = {};
    struct stat aliasCRCBefore = {};
    assert(stat((sourceDir + "/good").c_str(), &aliasDataBefore) == 0);
    assert(stat((sourceDir + "/good.crc").c_str(), &aliasCRCBefore) == 0);
    const string sourceAlias = sourceDir + "/.";
    assert(MMKV::backupOneToDirectory("good", sourceAlias, &sourceDir));
    assert(MMKV::backupAllToDirectory(sourceAlias, &sourceDir) == 1);
    assert(MMKV::restoreOneFromDirectory("good", sourceAlias, &sourceDir));
    assert(MMKV::restoreAllFromDirectory(sourceAlias, &sourceDir) == 1);
    struct stat aliasDataAfter = {};
    struct stat aliasCRCAfter = {};
    assert(stat((sourceDir + "/good").c_str(), &aliasDataAfter) == 0);
    assert(stat((sourceDir + "/good.crc").c_str(), &aliasCRCAfter) == 0);
    assert(aliasDataBefore.st_dev == aliasDataAfter.st_dev && aliasDataBefore.st_ino == aliasDataAfter.st_ino);
    assert(aliasCRCBefore.st_dev == aliasCRCAfter.st_dev && aliasCRCBefore.st_ino == aliasCRCAfter.st_ino);

    assert(MMKV::backupAllToDirectory(backupDir, &sourceDir) == 2);
    assert(isFileExist(backupDir + "/good"));
    assert(readTestFile(backupDir + "/good") == "data");
    assert(readTestFile(backupDir + "/good.crc") == metadataBytes);
    assert(readTestFile(backupDir + "/suffix.crc") == "suffix-data");
    assert(readTestFile(backupDir + "/suffix.crc.crc") == metadataBytes);
    assert(!isFileExist(backupDir + "/bad"));
    assert(!isFileExist(backupDir + "/hard"));
    assert(!MMKV::backupOneToDirectory("bad", backupDir, &sourceDir));
    assert(!MMKV::backupOneToDirectory("hard", backupDir, &sourceDir));

    assert(MMKV::restoreAllFromDirectory(backupDir, &restoreDir) == 2);
    assert(isFileExist(restoreDir + "/good"));
    assert(readTestFile(restoreDir + "/good") == "data");
    assert(readTestFile(restoreDir + "/good.crc") == metadataBytes);
    assert(readTestFile(restoreDir + "/suffix.crc") == "suffix-data");
    assert(readTestFile(restoreDir + "/suffix.crc.crc") == metadataBytes);
    assert(!MMKV::restoreOneFromDirectory("bad", sourceDir, &restoreDir));
    assert(!MMKV::restoreOneFromDirectory("hard", sourceDir, &restoreDir));
    assert(readTestFile(outsideHardlinkData) == "outside-hardlink-data");
    assert(readTestFile(outsideHardlinkCRC) == metadataBytes);

    const string specialID = "special/id";
    const string specialBackupDir = string(base) + "/special-backup";
    auto sourceNamespace = MMKV::nameSpace(sourceDir);
    auto specialMMKV = sourceNamespace.mmkvWithID(specialID);
    assert(specialMMKV);
    specialMMKV->clearAll();
    assert(specialMMKV->set("before", "value"));
    assert(sourceNamespace.backupOneToDirectory(specialID, specialBackupDir));
    assert(specialMMKV->set("after", "value"));
    assert(sourceNamespace.restoreOneFromDirectory(specialID, specialBackupDir));
    string specialValue;
    assert(specialMMKV->getString("value", specialValue));
    assert(specialValue == "before");
    specialMMKV->close();

    const string cachedID = "cached-backup-restore";
    const string cachedBackupDir = string(base) + "/cached-backup";
    auto cachedMMKV = MMKV::mmkvWithID(cachedID);
    assert(cachedMMKV);
    cachedMMKV->clearAll();
    assert(cachedMMKV->set("before", "value"));
    assert(MMKV::backupOneToDirectory(cachedID, cachedBackupDir));
    constexpr char crcTailMarker[] = "crc-tail-marker";
    constexpr off_t crcTailOffset = static_cast<off_t>(sizeof(MMKVMetaInfo) + 32);
    auto cachedBackupCRC = open((cachedBackupDir + "/" + cachedID + ".crc").c_str(), O_RDWR);
    assert(cachedBackupCRC >= 0);
    assert(pwrite(cachedBackupCRC, crcTailMarker, sizeof(crcTailMarker), crcTailOffset) ==
           static_cast<ssize_t>(sizeof(crcTailMarker)));
    close(cachedBackupCRC);
    assert(cachedMMKV->set("after", "value"));
    assert(MMKV::restoreOneFromDirectory(cachedID, cachedBackupDir));
    string restoredValue;
    assert(cachedMMKV->getString("value", restoredValue));
    assert(restoredValue == "before");
    array<char, sizeof(crcTailMarker)> restoredCRCTail = {};
    auto cachedLiveCRC = open((MMKV::getRootDir() + "/" + cachedID + ".crc").c_str(), O_RDONLY);
    assert(cachedLiveCRC >= 0);
    assert(pread(cachedLiveCRC, restoredCRCTail.data(), restoredCRCTail.size(), crcTailOffset) ==
           static_cast<ssize_t>(restoredCRCTail.size()));
    close(cachedLiveCRC);
    assert(memcmp(restoredCRCTail.data(), crcTailMarker, sizeof(crcTailMarker)) == 0);
    cachedMMKV->close();

    // A cached read-only instance has no dirty pages to publish and may be
    // backed by files that cannot be reopened for writing. Backup must keep
    // using the pinned read handles instead of requiring fsync/FlushFileBuffers
    // access that the instance itself does not have.
    const string readOnlyID = "cached-read-only-backup";
    const string readOnlyRoot = string(base) + "/read-only-root";
    const string readOnlyBackup = string(base) + "/read-only-backup";
    auto readOnlyNamespace = MMKV::nameSpace(readOnlyRoot);
    auto readOnlyWriter = readOnlyNamespace.mmkvWithID(readOnlyID);
    assert(readOnlyWriter);
    readOnlyWriter->clearAll();
    assert(readOnlyWriter->set("read-only-value", "value"));
    readOnlyWriter->sync(MMKV_SYNC);
    readOnlyWriter->close();
    assert(chmod((readOnlyRoot + "/" + readOnlyID).c_str(), 0400) == 0);
    assert(chmod((readOnlyRoot + "/" + readOnlyID + ".crc").c_str(), 0400) == 0);
    auto readOnlyMMKV = readOnlyNamespace.mmkvWithID(
        readOnlyID, static_cast<MMKVMode>(MMKV_SINGLE_PROCESS | MMKV_READ_ONLY));
    assert(readOnlyMMKV);
    string readOnlyValue;
    assert(readOnlyMMKV->getString("value", readOnlyValue));
    assert(readOnlyValue == "read-only-value");
    assert(readOnlyNamespace.backupOneToDirectory(readOnlyID, readOnlyBackup));
    readOnlyMMKV->close();

    // A lexical cache hit whose pinned files no longer match the live
    // mappings must fail closed. Neither backup nor restore may silently act
    // on replacement files while the cached object still owns the old pair.
    const string hintedID = "cached-hint-replacement";
    const string hintedRoot = string(base) + "/hinted-root";
    const string hintedBackup = string(base) + "/hinted-backup";
    auto hintedNamespace = MMKV::nameSpace(hintedRoot);
    auto hintedMMKV = hintedNamespace.mmkvWithID(hintedID);
    assert(hintedMMKV);
    hintedMMKV->clearAll();
    assert(hintedMMKV->set("mapped-value", "value"));
    assert(hintedNamespace.backupOneToDirectory(hintedID, hintedBackup));
    assert(rename((hintedRoot + "/" + hintedID).c_str(),
                  (hintedRoot + "/" + hintedID + ".mapped").c_str()) == 0);
    assert(rename((hintedRoot + "/" + hintedID + ".crc").c_str(),
                  (hintedRoot + "/" + hintedID + ".crc.mapped").c_str()) == 0);
    writeTestFile(hintedRoot + "/" + hintedID, "replacement-data");
    writeTestBytes(hintedRoot + "/" + hintedID + ".crc", &metadata, sizeof(metadata));
    assert(!hintedNamespace.backupOneToDirectory(hintedID, string(base) + "/must-not-back-up"));
    assert(!hintedNamespace.restoreOneFromDirectory(hintedID, hintedBackup));
    assert(readTestFile(hintedRoot + "/" + hintedID) == "replacement-data");
    hintedMMKV->close();

    // A lazy instance still owns the identity of its last mapped data file.
    // Matching metadata alone must not authorize a data-only replacement.
    const string lazyID = "cached-lazy-data-replacement";
    const string lazyRoot = string(base) + "/lazy-root";
    const string lazyBackup = string(base) + "/lazy-backup";
    auto lazyNamespace = MMKV::nameSpace(lazyRoot);
    auto lazyMMKV = lazyNamespace.mmkvWithID(lazyID);
    assert(lazyMMKV);
    lazyMMKV->clearAll();
    assert(lazyMMKV->set("mapped-value", "value"));
    assert(lazyNamespace.backupOneToDirectory(lazyID, lazyBackup));
    lazyMMKV->clearMemoryCache();
    assert(rename((lazyRoot + "/" + lazyID).c_str(),
                  (lazyRoot + "/" + lazyID + ".mapped").c_str()) == 0);
    writeTestFile(lazyRoot + "/" + lazyID, "lazy-replacement-data");
    assert(!lazyNamespace.backupOneToDirectory(lazyID, string(base) + "/must-not-back-up-lazy"));
    assert(!lazyNamespace.restoreOneFromDirectory(lazyID, lazyBackup));
    assert(readTestFile(lazyRoot + "/" + lazyID) == "lazy-replacement-data");
    lazyMMKV->close();

    // A replacement pair can itself belong to another cached instance. A
    // lexical hit for A must be checked against A, rather than falling through
    // the cache scan and silently selecting B by file identity.
    const string swappedRoot = string(base) + "/swapped-root";
    const string swappedBackup = string(base) + "/swapped-backup";
    const string firstID = "cached-first";
    const string secondID = "cached-second";
    auto swappedNamespace = MMKV::nameSpace(swappedRoot);
    auto firstMMKV = swappedNamespace.mmkvWithID(firstID);
    auto secondMMKV = swappedNamespace.mmkvWithID(secondID);
    assert(firstMMKV && secondMMKV);
    firstMMKV->clearAll();
    secondMMKV->clearAll();
    assert(firstMMKV->set("first-value", "value"));
    assert(secondMMKV->set("second-value", "value"));
    assert(swappedNamespace.backupOneToDirectory(firstID, swappedBackup));
    assert(rename((swappedRoot + "/" + firstID).c_str(),
                  (swappedRoot + "/" + firstID + ".original").c_str()) == 0);
    assert(rename((swappedRoot + "/" + firstID + ".crc").c_str(),
                  (swappedRoot + "/" + firstID + ".crc.original").c_str()) == 0);
    assert(rename((swappedRoot + "/" + secondID).c_str(),
                  (swappedRoot + "/" + firstID).c_str()) == 0);
    assert(rename((swappedRoot + "/" + secondID + ".crc").c_str(),
                  (swappedRoot + "/" + firstID + ".crc").c_str()) == 0);
    assert(!swappedNamespace.backupOneToDirectory(firstID, string(base) + "/must-not-back-up-second"));
    assert(!swappedNamespace.restoreOneFromDirectory(firstID, swappedBackup));
    const string swappedRootAlias = swappedRoot + "/.";
    assert(MMKV::restoreAllFromDirectory(swappedBackup, &swappedRootAlias) == 0);
    string firstValue;
    string secondValue;
    assert(firstMMKV->getString("value", firstValue) && firstValue == "first-value");
    assert(secondMMKV->getString("value", secondValue) && secondValue == "second-value");
    firstMMKV->close();
    secondMMKV->close();

    // Android can retain an old on-disk basename when legacy migration is
    // blocked while indexing the live instance under the original mmapID.
    // Simulate that cache-key/basename mismatch and prove restore-all still
    // identifies the live mappings. Rename the destination root after its
    // handles are pinned as well: cached reload must stay on those handles and
    // must not create or map files beneath the replacement path. The /.
    // source spelling also verifies a lexical alias of the pinned source.
    const string mismatchID = "cached-base-key-mismatch";
    const string mismatchSourceDir = string(base) + "/mismatch-source";
    const string mismatchDestinationDir = string(base) + "/mismatch-destination";
    const string mismatchPinnedDestinationDir = string(base) + "/mismatch-destination-pinned";
    auto mismatchSourceNamespace = MMKV::nameSpace(mismatchSourceDir);
    auto mismatchSource = mismatchSourceNamespace.mmkvWithID(mismatchID);
    assert(mismatchSource);
    mismatchSource->clearAll();
    assert(mismatchSource->set("from-source", "value"));
    mismatchSource->close();

    auto mismatchDestinationNamespace = MMKV::nameSpace(mismatchDestinationDir);
    auto mismatchDestination = mismatchDestinationNamespace.mmkvWithID(mismatchID, MMKV_MULTI_PROCESS);
    assert(mismatchDestination);
    mismatchDestination->clearAll();
    assert(mismatchDestination->set("stale-destination", "value"));
    // Leave only the metadata mapping live. Cache recognition must still keep
    // this lazy instance bound to the pinned destination pair.
    mismatchDestination->clearMemoryCache();

    auto cacheEntry = find_if(g_instanceDic->begin(), g_instanceDic->end(), [&](const auto &entry) {
        return entry.second == mismatchDestination;
    });
    assert(cacheEntry != g_instanceDic->end());
    auto originalCacheKey = cacheEntry->first;
    auto cacheNode = g_instanceDic->extract(cacheEntry);
    const string mismatchedCacheKey = originalCacheKey + "-legacy-basename-mismatch";
    assert(g_instanceDic->find(mismatchedCacheKey) == g_instanceDic->end());
    cacheNode.key() = mismatchedCacheKey;
    assert(g_instanceDic->insert(std::move(cacheNode)).inserted);

    OneShotLogHandler mismatchHandler("restoreOneFromDirectoryWithHandles", "restore one cached", [&] {
        assert(rename(mismatchDestinationDir.c_str(), mismatchPinnedDestinationDir.c_str()) == 0);
        error_code replacementError;
        fs::create_directories(mismatchDestinationDir, replacementError);
        assert(!replacementError);
        writeTestFile(mismatchDestinationDir + "/replacement-marker", "replacement-root-must-stay-isolated");
    });
    MMKV::registerHandler(&mismatchHandler);
    auto mismatchRestoreCount =
        mismatchDestinationNamespace.restoreAllFromDirectory(mismatchSourceDir + "/.");
    MMKV::unRegisterHandler();
    assert(mismatchHandler.fired);
    assert(mismatchRestoreCount == 1);
    string mismatchRestoredValue;
    assert(mismatchDestination->getString("value", mismatchRestoredValue));
    assert(mismatchRestoredValue == "from-source");
    assert(readTestFile(mismatchDestinationDir + "/replacement-marker") == "replacement-root-must-stay-isolated");
    assert(!fs::exists(mismatchDestinationDir + "/" + mismatchID));
    assert(!fs::exists(mismatchDestinationDir + "/" + mismatchID + ".crc"));
    assert(fs::exists(mismatchPinnedDestinationDir + "/" + mismatchID));
    assert(fs::exists(mismatchPinnedDestinationDir + "/" + mismatchID + ".crc"));

    cacheNode = g_instanceDic->extract(mismatchedCacheKey);
    assert(!cacheNode.empty());
    cacheNode.key() = originalCacheKey;
    assert(g_instanceDic->insert(std::move(cacheNode)).inserted);
    mismatchDestination->close();

    const string outsideDestination = string(base) + "/outside-destination";
    const string redirectedDestination = string(base) + "/redirected-destination";
    fs::create_directories(outsideDestination, createError);
    assert(!createError);
    assert(symlink(outsideDestination.c_str(), redirectedDestination.c_str()) == 0);
    const string unsafeDestination = redirectedDestination + "/must-not-exist";
    assert(MMKV::backupAllToDirectory(unsafeDestination, &sourceDir) == 0);
    assert(MMKV::restoreAllFromDirectory(backupDir, &unsafeDestination) == 0);
    assert(!MMKV::backupOneToDirectory("good", unsafeDestination, &sourceDir));
    assert(!MMKV::backupOneToDirectory("bad/id", unsafeDestination, &sourceDir));
    assert(!MMKV::restoreOneFromDirectory("good", backupDir, &unsafeDestination));
    assert(!fs::exists(outsideDestination + "/must-not-exist"));

    const string embeddedNullDestination = backupDir + string("\0ignored", 8);
    assert(MMKV::backupAllToDirectory(embeddedNullDestination, &sourceDir) == 0);
    assert(MMKV::restoreAllFromDirectory(backupDir, &embeddedNullDestination) == 0);
    assert(!MMKV::backupOneToDirectory("good", embeddedNullDestination, &sourceDir));
    assert(!MMKV::restoreOneFromDirectory("good", backupDir, &embeddedNullDestination));
    const string embeddedNullSource = sourceDir + string("\0ignored", 8);
    assert(MMKV::backupAllToDirectory(backupDir, &embeddedNullSource) == 0);
    assert(!MMKV::restoreOneFromDirectory("good", embeddedNullSource, &restoreDir));

    const string truncatedSourceDir = string(base) + "/truncated-source";
    const string unchangedDestinationDir = string(base) + "/unchanged-destination";
    fs::create_directories(truncatedSourceDir, createError);
    assert(!createError);
    fs::create_directories(unchangedDestinationDir, createError);
    assert(!createError);
    writeTestFile(truncatedSourceDir + "/victim", "replacement");
    writeTestFile(truncatedSourceDir + "/victim.crc", "x");
    writeTestFile(unchangedDestinationDir + "/victim", "original");
    writeTestBytes(unchangedDestinationDir + "/victim.crc", &metadata, sizeof(metadata));
    assert(!MMKV::restoreOneFromDirectory("victim", truncatedSourceDir, &unchangedDestinationDir));
    assert(readTestFile(unchangedDestinationDir + "/victim") == "original");
    assert(readTestFile(unchangedDestinationDir + "/victim.crc") == metadataBytes);

    // Writable destination handles must not cross the pinned directory
    // boundary through a hardlink to an outside inode.
    const string hardlinkSourceDir = string(base) + "/hardlink-source";
    const string hardlinkDestinationDir = string(base) + "/hardlink-destination";
    const string outsideHardlinkTarget = string(base) + "/outside-hardlink-target";
    fs::create_directories(hardlinkSourceDir, createError);
    assert(!createError);
    fs::create_directories(hardlinkDestinationDir, createError);
    assert(!createError);
    writeTestFile(hardlinkSourceDir + "/victim", "replacement");
    writeTestBytes(hardlinkSourceDir + "/victim.crc", &metadata, sizeof(metadata));
    writeTestFile(outsideHardlinkTarget, "outside-original");
    assert(link(outsideHardlinkTarget.c_str(), (hardlinkDestinationDir + "/victim").c_str()) == 0);
    writeTestBytes(hardlinkDestinationDir + "/victim.crc", &metadata, sizeof(metadata));
    assert(!MMKV::restoreOneFromDirectory("victim", hardlinkSourceDir, &hardlinkDestinationDir));
    assert(readTestFile(outsideHardlinkTarget) == "outside-original");
    assert(readTestFile(hardlinkDestinationDir + "/victim.crc") == metadataBytes);

    error_code cleanupError;
    fs::remove_all(base, cleanupError);
    assert(!cleanupError);
    printf("test backup/restore no-follow: passed\n");
}

void testAllDirectoryPinnedRoots(const string &testRoot) {
    string baseTemplate = testRoot + "/mmkv-all-pinned-XXXXXX";
    vector<char> mutableTemplate(baseTemplate.begin(), baseTemplate.end());
    mutableTemplate.push_back('\0');
    auto base = mkdtemp(mutableTemplate.data());
    assert(base);

    MMKVMetaInfo metadata;
    auto createSpecialPair = [&](const string &root, const char *value) {
        error_code createError;
        fs::create_directories(root + "/specialCharacter", createError);
        assert(!createError);
        writeTestFile(root + "/specialCharacter/special", value);
        writeTestBytes(root + "/specialCharacter/special.crc", &metadata, sizeof(metadata));
    };
    auto installReplacementRoots = [&](const string &source,
                                       const string &pinnedSource,
                                       const string &destination,
                                       const string &pinnedDestination) {
        assert(rename(source.c_str(), pinnedSource.c_str()) == 0);
        assert(rename(destination.c_str(), pinnedDestination.c_str()) == 0);
        error_code createError;
        fs::create_directories(source, createError);
        assert(!createError);
        fs::create_directories(destination, createError);
        assert(!createError);
        createSpecialPair(source, "replacement-special");
        writeTestFile(destination + "/replacement-marker", "keep");
    };

    {
        const string source = string(base) + "/backup-source";
        const string pinnedSource = string(base) + "/backup-source-pinned";
        const string destination = string(base) + "/backup-destination";
        const string pinnedDestination = string(base) + "/backup-destination-pinned";
        error_code createError;
        fs::create_directories(source, createError);
        assert(!createError);
        fs::create_directories(destination, createError);
        assert(!createError);
        createSpecialPair(source, "original-special");

        auto sourceNamespace = MMKV::nameSpace(source);
        auto gate = sourceNamespace.mmkvWithID("gate");
        assert(gate);
        gate->clearAll();
        assert(gate->set("backup-value", "value"));

        OneShotLogHandler handler("backupOneToDirectoryWithHandles", "finish backup one mmkv", [&] {
            installReplacementRoots(source, pinnedSource, destination, pinnedDestination);
        });
        MMKV::registerHandler(&handler);
        auto count = sourceNamespace.backupAllToDirectory(destination);
        MMKV::unRegisterHandler();
        assert(handler.fired);
        assert(count == 2);
        assert(readTestFile(pinnedDestination + "/specialCharacter/special") == "original-special");
        assert(readTestFile(destination + "/replacement-marker") == "keep");
        assert(!fs::exists(destination + "/specialCharacter/special"));
        gate->close();
    }

    {
        const string source = string(base) + "/restore-source";
        const string pinnedSource = string(base) + "/restore-source-pinned";
        const string destination = string(base) + "/restore-destination";
        const string pinnedDestination = string(base) + "/restore-destination-pinned";
        error_code createError;
        fs::create_directories(source, createError);
        assert(!createError);
        fs::create_directories(destination, createError);
        assert(!createError);
        createSpecialPair(source, "original-special");

        auto sourceNamespace = MMKV::nameSpace(source);
        auto sourceGate = sourceNamespace.mmkvWithID("gate");
        assert(sourceGate);
        sourceGate->clearAll();
        assert(sourceGate->set("source-value", "value"));
        sourceGate->close();

        auto destinationNamespace = MMKV::nameSpace(destination);
        auto destinationGate = destinationNamespace.mmkvWithID("gate");
        assert(destinationGate);
        destinationGate->clearAll();
        assert(destinationGate->set("destination-value", "value"));

        OneShotLogHandler handler("restoreOneFromDirectoryWithHandles", "finish restore one mmkv", [&] {
            installReplacementRoots(source, pinnedSource, destination, pinnedDestination);
        });
        MMKV::registerHandler(&handler);
        auto count = destinationNamespace.restoreAllFromDirectory(source);
        MMKV::unRegisterHandler();
        assert(handler.fired);
        assert(count == 2);
        string restoredValue;
        assert(destinationGate->getString("value", restoredValue));
        assert(restoredValue == "source-value");
        assert(readTestFile(pinnedDestination + "/specialCharacter/special") == "original-special");
        assert(readTestFile(destination + "/replacement-marker") == "keep");
        assert(!fs::exists(destination + "/specialCharacter/special"));
        destinationGate->close();
    }

    error_code cleanupError;
    fs::remove_all(base, cleanupError);
    assert(!cleanupError);
    printf("test all-directory pinned roots: passed\n");
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

    const char *overrideRoot = getenv("MMKV_TEST_ROOT");
    error_code tempError;
    string testRoot = (overrideRoot && overrideRoot[0] != '\0') ? overrideRoot : fs::temp_directory_path(tempError).string();
    assert(!tempError);
    error_code createError;
    fs::create_directories(testRoot, createError);
    assert(!createError && fs::is_directory(testRoot));
    testRoot = absolutePath(testRoot);
    assert(setenv("TMPDIR", testRoot.c_str(), 1) == 0);

    string rootDir = testRoot + "/mmkv";
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
    testLongDirectoryWalk(testRoot);
    testMayflyMappedFileIdentity(testRoot);
    testPinnedDirectoryHandles(testRoot);
    testPairFailureRollback(testRoot);
    testBackupRestoreNoFollow(testRoot);
    testAllDirectoryPinnedRoots(testRoot);
}
