/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License.
 */

#include <MMKV/MMKV.h>

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

namespace {

atomic<uint32_t> g_msyncCallCount{0};
atomic<uint32_t> g_failMSyncCall{0};

using MSyncFunction = int (*)(void *, size_t, int);

void require(bool condition) {
    if (!condition) {
        abort();
    }
}

void requirePathWithin(const fs::path &path, const fs::path &root) {
    auto relativePath = path.lexically_relative(root);
    require(!relativePath.empty() && !relativePath.is_absolute());
    for (const auto &component : relativePath) {
        require(component != "..");
    }
}

MSyncFunction realMSync() {
    static auto function = reinterpret_cast<MSyncFunction>(dlsym(RTLD_NEXT, "msync"));
    return function;
}

void assertValue(MMKV *kv, const char *key, const char *expected) {
    string value;
    require(kv->getString(key, value));
    require(value == expected);
}

void failMSyncCall(uint32_t call) {
    require(call > 0);
    g_msyncCallCount = 0;
    g_failMSyncCall = call;
}

void requireInjectedFailureWasReached() {
    require(g_failMSyncCall.load() == 0);
}

void populateRollbackValues(MMKV *kv) {
    kv->clearAll();
    require(kv->set("value-a", "a"));
    require(kv->set("value-b", "b"));
    require(kv->set("value-c", "c"));
    kv->sync(MMKV_SYNC);
}

void assertRollbackValues(MMKV *kv) {
    assertValue(kv, "a", "value-a");
    assertValue(kv, "b", "value-b");
    assertValue(kv, "c", "value-c");
}

void runFullWriteRollback(const string &mmapID, const string *cryptKey, uint32_t failedMSyncCall) {
    auto kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey);
    require(kv);
    populateRollbackValues(kv);

    failMSyncCall(failedMSyncCall);
    require(!kv->removeValuesForKeys(vector<string>{"a", "b"}));
    requireInjectedFailureWasReached();

    // The failed rewrite must restore the prior logical state and rebuild the
    // dictionary, writer, and encryption stream for subsequent operations.
    assertRollbackValues(kv);
    require(kv->set("value-d", "d"));
    kv->sync(MMKV_SYNC);
    kv->close();

    kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey);
    require(kv);
    assertRollbackValues(kv);
    assertValue(kv, "d", "value-d");
    kv->close();
}

void runClearRollback(const string &mmapID, const string *cryptKey) {
    auto kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey);
    require(kv);
    populateRollbackValues(kv);

    // clearAll(true) changes logical metadata without truncating the data mapping.
    // Fail that metadata sync and verify the old state survives reload and reopen.
    failMSyncCall(1);
    kv->clearAll(true);
    requireInjectedFailureWasReached();
    assertRollbackValues(kv);
    require(kv->set("value-d", "d"));
    kv->sync(MMKV_SYNC);
    kv->close();

    kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey);
    require(kv);
    assertRollbackValues(kv);
    assertValue(kv, "d", "value-d");
    kv->close();
}

void runExpirationTransitionRollback(const string &mmapID, const string *cryptKey) {
    auto kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey);
    require(kv);
    populateRollbackValues(kv);
    require(!kv->isExpirationEnabled());

    // Data is flushed before metadata. Fail the metadata publication and make
    // sure old-format values remain paired with expiration-disabled metadata.
    failMSyncCall(2);
    require(!kv->enableAutoKeyExpire(3600));
    requireInjectedFailureWasReached();
    require(!kv->isExpirationEnabled());
    assertRollbackValues(kv);

    require(kv->enableAutoKeyExpire(3600));
    require(kv->isExpirationEnabled());
    assertRollbackValues(kv);

    // The inverse transition must likewise keep timestamp-bearing values
    // paired with expiration-enabled metadata after a failed sync.
    failMSyncCall(2);
    require(!kv->disableAutoKeyExpire());
    requireInjectedFailureWasReached();
    require(kv->isExpirationEnabled());
    assertRollbackValues(kv);
    kv->close();

    kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey);
    require(kv);
    assertRollbackValues(kv);
    require(kv->isExpirationEnabled());
    require(kv->disableAutoKeyExpire());
    require(!kv->isExpirationEnabled());
    assertRollbackValues(kv);
    kv->close();
}

void runEmptyExpirationTransitionRollback(const string &mmapID, const string *cryptKey) {
    auto kv = MMKV::mmkvWithID(mmapID, MMKV_SINGLE_PROCESS, cryptKey);
    require(kv);
    kv->clearAll();
    require(kv->enableCompareBeforeSet());

    failMSyncCall(1);
    require(!kv->enableAutoKeyExpire(3600));
    requireInjectedFailureWasReached();
    require(!kv->isExpirationEnabled());
    require(kv->isCompareBeforeSetEnabled());

    require(kv->enableAutoKeyExpire(3600));
    require(kv->isExpirationEnabled());
    failMSyncCall(1);
    require(!kv->disableAutoKeyExpire());
    requireInjectedFailureWasReached();
    require(kv->isExpirationEnabled());
    kv->close();
}

} // namespace

// MemoryFile.cpp is linked into this test executable from the static core target,
// so its msync calls resolve here. Release libraries contain no fault-injection hook.
extern "C" int msync(void *address, size_t length, int flags) {
    auto call = g_msyncCallCount.fetch_add(1) + 1;
    if (g_failMSyncCall.load() == call) {
        g_failMSyncCall = 0;
        errno = EIO;
        return -1;
    }
    auto function = realMSync();
    if (!function) {
        errno = ENOSYS;
        return -1;
    }
    return function(address, length, flags);
}

int main() {
    const char *overrideRoot = getenv("MMKV_TEST_ROOT");
    require(overrideRoot && overrideRoot[0] != '\0');
    error_code error;
    auto workingDirectory = fs::canonical(fs::current_path(error), error);
    require(!error);
    auto testRoot = fs::weakly_canonical(fs::path(overrideRoot), error);
    require(!error);
    requirePathWithin(testRoot, workingDirectory);
    fs::create_directories(testRoot, error);
    require(!error);
    testRoot = fs::canonical(testRoot, error);
    require(!error);
    requirePathWithin(testRoot, workingDirectory);

    auto pathTemplate = (testRoot / "mmkv-crypto-rollback-XXXXXX").string();
    vector<char> mutableTemplate(pathTemplate.begin(), pathTemplate.end());
    mutableTemplate.push_back('\0');
    auto uniqueRoot = mkdtemp(mutableTemplate.data());
    require(uniqueRoot);

    auto storageRoot = (fs::path(uniqueRoot) / "storage").string();
    MMKV::initializeMMKV(storageRoot);

    const char binaryKeyBytes[] = {'0', '1', '2', '3', '\0', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    const string binaryKey(binaryKeyBytes, sizeof(binaryKeyBytes));
#ifndef MMKV_DISABLE_CRYPT
    const string *key = &binaryKey;
#else
    const string *key = nullptr;
#endif

    runFullWriteRollback("full-write-data-sync-rollback", key, 1);
    runFullWriteRollback("full-write-meta-sync-rollback", key, 2);
    runClearRollback("clear-meta-sync-rollback", key);
    runExpirationTransitionRollback("expiration-transition-rollback", key);
    runEmptyExpirationTransitionRollback("empty-expiration-transition-rollback", nullptr);

    fs::remove_all(uniqueRoot, error);
    require(!error);
    return 0;
}
