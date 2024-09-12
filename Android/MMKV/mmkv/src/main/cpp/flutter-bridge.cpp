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

#include "MMKVPredef.h"

#ifndef MMKV_DISABLE_FLUTTER

#    include "MMKV.h"
#    include "MMKVLog.h"
#    include <cstdint>
#    include <string>

using namespace mmkv;
using namespace std;

namespace mmkv {
extern int g_android_api;
extern string g_android_tmpDir;
}

#    define MMKV_EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))

using LogCallback_t = void (*)(uint32_t level, const char *file, int32_t line, const char *funcname, const char *message);
LogCallback_t g_logCallback = nullptr;

static void myLogHandler(MMKVLogLevel level, const char *file, int line, const char *function, MMKVLog_t message) {
    if (g_logCallback) {
        g_logCallback(level, file, line, function, message.c_str());
    }
}

MMKV_EXPORT void *mmkvInitialize_v2(const char *rootDir, const char *cacheDir, int32_t sdkInt, int32_t logLevel, LogCallback_t callback) {
    if (!rootDir) {
        return nullptr;
    }
    if (cacheDir) {
        g_android_tmpDir = string(cacheDir);
    }

    g_android_api = sdkInt;
#ifdef MMKV_STL_SHARED
    MMKVInfo("current API level = %d, libc++_shared=%d", g_android_api, MMKV_STL_SHARED);
#else
    MMKVInfo("current API level = %d, libc++_shared=?", g_android_api);
#endif

    if (callback) {
        g_logCallback = callback;
        MMKV::initializeMMKV(rootDir, (MMKVLogLevel) logLevel, myLogHandler);
    } else {
        MMKV::initializeMMKV(rootDir, (MMKVLogLevel) logLevel);
    }
    return (void *) MMKV::getRootDir().c_str();
}

MMKV_EXPORT void mmkvInitialize_v1(const char *rootDir, const char *cacheDir, int32_t sdkInt, int32_t logLevel) {
    mmkvInitialize_v2(rootDir, cacheDir, sdkInt, logLevel, nullptr);
}

MMKV_EXPORT void mmkvInitialize(const char *rootDir, int32_t logLevel) {
    mmkvInitialize_v2(rootDir, nullptr, 0, logLevel, nullptr);
}

MMKV_EXPORT void *getMMKVWithID(const char *mmapID, int32_t mode, const char *cryptKey, const char *rootPath, size_t expectedCapacity) {
    MMKV *kv = nullptr;
    if (!mmapID) {
        return kv;
    }
    string str = mmapID;

    bool done = false;
    if (cryptKey) {
        string crypt = cryptKey;
        if (crypt.length() > 0) {
            if (rootPath) {
                string path = rootPath;
                kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, &crypt, &path, expectedCapacity);
            } else {
                kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, &crypt, nullptr, expectedCapacity);
            }
            done = true;
        }
    }
    if (!done) {
        if (rootPath) {
            string path = rootPath;
            kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, nullptr, &path, expectedCapacity);
        } else {
            kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, nullptr, nullptr, expectedCapacity);
        }
    }

    return kv;
}

MMKV_EXPORT void *getDefaultMMKV(int32_t mode, const char *cryptKey) {
    MMKV *kv = nullptr;

    if (cryptKey) {
        string crypt = cryptKey;
        if (crypt.length() > 0) {
            kv = MMKV::defaultMMKV((MMKVMode) mode, &crypt);
        }
    }
    if (!kv) {
        kv = MMKV::defaultMMKV((MMKVMode) mode, nullptr);
    }

    return kv;
}

MMKV_EXPORT const char *mmapID(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->mmapID().c_str();
    }
    return nullptr;
}

MMKV_EXPORT bool encodeBool(void *handle, const char *oKey, bool value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((bool) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeBool_v2(void *handle, const char *oKey, bool value, uint32_t expiration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((bool) value, key, expiration);
    }
    return false;
}

MMKV_EXPORT bool decodeBool(void *handle, const char *oKey, bool defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getBool(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeInt32(void *handle, const char *oKey, int32_t value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((int32_t) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeInt32_v2(void *handle, const char *oKey, int32_t value, uint32_t expiration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((int32_t) value, key, expiration);
    }
    return false;
}

MMKV_EXPORT int32_t decodeInt32(void *handle, const char *oKey, int32_t defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getInt32(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeInt64(void *handle, const char *oKey, int64_t value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((int64_t) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeInt64_v2(void *handle, const char *oKey, int64_t value, uint32_t expiration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((int64_t) value, key, expiration);
    }
    return false;
}

MMKV_EXPORT int64_t decodeInt64(void *handle, const char *oKey, int64_t defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getInt64(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeDouble(void *handle, const char *oKey, double value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((double) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeDouble_v2(void *handle, const char *oKey, double value, uint32_t expiration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((double) value, key, expiration);
    }
    return false;
}

MMKV_EXPORT double decodeDouble(void *handle, const char *oKey, double defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getDouble(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeBytes(void *handle, const char *oKey, void *oValue, uint64_t length) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        if (oValue) {
            auto value = MMBuffer(oValue, static_cast<size_t>(length), MMBufferNoCopy);
            return kv->set(value, key);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

MMKV_EXPORT bool encodeBytes_v2(void *handle, const char *oKey, void *oValue, uint64_t length, uint32_t expiration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        if (oValue) {
            auto value = MMBuffer(oValue, static_cast<size_t>(length), MMBufferNoCopy);
            return kv->set(value, key, expiration);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

MMKV_EXPORT void *decodeBytes(void *handle, const char *oKey, uint64_t *lengthPtr) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        mmkv::MMBuffer value;
        auto hasValue = kv->getBytes(key, value);
        if (hasValue) {
            if (value.length() > 0) {
                if (value.isStoredOnStack()) {
                    auto result = malloc(value.length());
                    if (result) {
                        memcpy(result, value.getPtr(), value.length());
                        *lengthPtr = value.length();
                    }
                    return result;
                }
                void *result = value.getPtr();
                *lengthPtr = value.length();
                value.detach();
                return result;
            }
            *lengthPtr = 0;
            // this ptr is intended for checking existence of the value
            // don't free this ptr
            return value.getPtr();
        }
    }
    return nullptr;
}

#    ifndef MMKV_DISABLE_CRYPT

MMKV_EXPORT bool reKey(void *handle, char *oKey, uint64_t length) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        if (oKey && length > 0) {
            string key(oKey, length);
            return kv->reKey(key);
        } else {
            return kv->reKey(string());
        }
    }
    return false;
}

MMKV_EXPORT void *cryptKey(void *handle, uint64_t *lengthPtr) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && lengthPtr) {
        auto cryptKey = kv->cryptKey();
        if (cryptKey.length() > 0) {
            auto ptr = malloc(cryptKey.length());
            if (ptr) {
                memcpy(ptr, cryptKey.data(), cryptKey.length());
                *lengthPtr = cryptKey.length();
                return ptr;
            }
        }
    }
    return nullptr;
}

MMKV_EXPORT void checkReSetCryptKey(void *handle, char *oKey, uint64_t length) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        if (oKey && length > 0) {
            string key(oKey, length);
            kv->checkReSetCryptKey(&key);
        } else {
            kv->checkReSetCryptKey(nullptr);
        }
    }
}

#    endif // MMKV_DISABLE_CRYPT

MMKV_EXPORT uint32_t valueSize(void *handle, char *oKey, bool actualSize) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key(oKey);
        auto ret = kv->getValueSize(key, actualSize);
        return static_cast<uint32_t>(ret);
    }
    return 0;
}

MMKV_EXPORT int32_t writeValueToNB(void *handle, char *oKey, void *pointer, uint32_t size) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key(oKey);
        return kv->writeValueToBuffer(key, pointer, size);
    }
    return -1;
}

MMKV_EXPORT uint64_t allKeys(void *handle, char ***keyArrayPtr, uint32_t **sizeArrayPtr, bool filterExpire) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        auto keys = kv->allKeys(filterExpire);
        if (!keys.empty()) {
            auto keyArray = (char **) malloc(keys.size() * sizeof(void *));
            auto sizeArray = (uint32_t *) malloc(keys.size() * sizeof(uint32_t *));
            if (!keyArray || !sizeArray) {
                free(keyArray);
                free(sizeArray);
                return 0;
            }
            *keyArrayPtr = keyArray;
            *sizeArrayPtr = sizeArray;

            for (size_t index = 0; index < keys.size(); index++) {
                auto &key = keys[index];
                sizeArray[index] = static_cast<uint32_t>(key.length());
                keyArray[index] = (char *) malloc(key.length());
                if (keyArray[index]) {
                    memcpy(keyArray[index], key.data(), key.length());
                }
            }
        }
        return keys.size();
    }
    return 0;
}

MMKV_EXPORT bool containsKey(void *handle, char *oKey) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key(oKey);
        return kv->containsKey(key);
    }
    return false;
}

MMKV_EXPORT uint64_t count(void *handle, bool filterExpire) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->count(filterExpire);
    }
    return 0;
}

MMKV_EXPORT uint64_t totalSize(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->totalSize();
    }
    return 0;
}

MMKV_EXPORT uint64_t actualSize(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->actualSize();
    }
    return 0;
}

MMKV_EXPORT void removeValueForKey(void *handle, char *oKey) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key(oKey);
        kv->removeValueForKey(key);
    }
}

MMKV_EXPORT void removeValuesForKeys(void *handle, char **keyArray, uint32_t *sizeArray, uint64_t count) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && keyArray && sizeArray && count > 0) {
        vector<string> arrKeys;
        arrKeys.reserve(count);
        for (uint64_t index = 0; index < count; index++) {
            if (sizeArray[index] > 0 && keyArray[index]) {
                arrKeys.emplace_back(keyArray[index], sizeArray[index]);
            }
        }
        if (!arrKeys.empty()) {
            kv->removeValuesForKeys(arrKeys);
        }
    }
}

MMKV_EXPORT void clearAll(void *handle, bool keepSpace) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        kv->clearAll(keepSpace);
    }
}

MMKV_EXPORT void mmkvSync(void *handle, bool sync) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        kv->sync((SyncFlag) sync);
    }
}

MMKV_EXPORT void clearMemoryCache(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        kv->clearMemoryCache();
    }
}

MMKV_EXPORT int32_t pageSize() {
    return static_cast<int32_t>(DEFAULT_MMAP_SIZE);
}

MMKV_EXPORT const char *version() {
    return MMKV_VERSION;
}

MMKV_EXPORT void trim(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        kv->trim();
    }
}

MMKV_EXPORT void mmkvClose(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        kv->close();
    }
}

MMKV_EXPORT void mmkvMemcpy(void *dst, const void *src, uint64_t size) {
    memcpy(dst, src, size);
}

MMKV_EXPORT bool backupOne(const char *mmapID, const char *dstDir, const char *rootPath) {
    if (rootPath) {
        auto root = string(rootPath);
        if (root.length() > 0) {
            return MMKV::backupOneToDirectory(mmapID, dstDir, &root);
        }
    }
    return MMKV::backupOneToDirectory(mmapID, dstDir);
}

MMKV_EXPORT bool restoreOne(const char *mmapID, const char *srcDir, const char *rootPath) {
    if (rootPath) {
        auto root = string(rootPath);
        if (root.length() > 0) {
            return MMKV::restoreOneFromDirectory(mmapID, srcDir, &root);
        }
    }
    return MMKV::restoreOneFromDirectory(mmapID, srcDir);
}

MMKV_EXPORT uint64_t backupAll(const char *dstDir/*, const char *rootPath*/) {
    // historically Android mistakenly use mmapKey as mmapID
    // makes everything tricky with customize root
    /*if (rootPath) {
        auto root = string(rootPath);
        if (root.length() > 0) {
            return MMKV::backupAllToDirectory(dstDir, &root);
        }
    }*/
    return MMKV::backupAllToDirectory(dstDir);
}

MMKV_EXPORT uint64_t restoreAll(const char *srcDir/*, const char *rootPath*/) {
    // historically Android mistakenly use mmapKey as mmapID
    // makes everything tricky with customize root
    /*if (rootPath) {
        auto root = string(rootPath);
        if (root.length() > 0) {
            return MMKV::restoreAllFromDirectory(srcDir, &root);
        }
    }*/
    return MMKV::restoreAllFromDirectory(srcDir);
}

MMKV_EXPORT bool enableAutoExpire(void *handle, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->enableAutoKeyExpire(expireDuration);
    }
    return false;
}

MMKV_EXPORT bool disableAutoExpire(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->disableAutoKeyExpire();
    }
    return false;
}

MMKV_EXPORT bool enableCompareBeforeSet(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->enableCompareBeforeSet();
    }
    return false;
}

MMKV_EXPORT bool disableCompareBeforeSet(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->disableCompareBeforeSet();
    }
    return false;
}

MMKV_EXPORT bool removeStorage(const char *mmapID, const char *rootPath) {
    if (rootPath) {
        auto root = string(rootPath);
        if (root.length() > 0) {
            return MMKV::removeStorage(mmapID, &root);
        }
    }
    return MMKV::removeStorage(mmapID, nullptr);
}

MMKV_EXPORT bool isMultiProcess(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->isMultiProcess();
    }
    return false;
}

MMKV_EXPORT bool isReadOnly(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        return kv->isReadOnly();
    }
    return false;
}

using ErrorCallback_t = int (*)(const char *mmapID, int32_t errorType);
static ErrorCallback_t g_errorCallback = nullptr;

static MMKVRecoverStrategic myErrorHandler(const std::string &mmapID, MMKVErrorType errorType) {
    if (g_errorCallback) {
        return (MMKVRecoverStrategic) g_errorCallback(mmapID.c_str(), errorType);
    }
    return OnErrorDiscard;
}

MMKV_EXPORT void registerErrorHandler(ErrorCallback_t callback) {
    g_errorCallback = callback;
    if (callback) {
        MMKV::registerErrorHandler(myErrorHandler);
    } else {
        MMKV::unRegisterErrorHandler();
    }
}

using ContenctChangeCallback_t = void (*)(const char *mmapID);
static ContenctChangeCallback_t g_contentChanceCallback = nullptr;

static void myContentChangeHandler(const std::string &mmapID) {
    if (g_contentChanceCallback) {
        g_contentChanceCallback(mmapID.c_str());
    }
}

MMKV_EXPORT void registerContentChangeNotify(ContenctChangeCallback_t callback) {
    g_contentChanceCallback = callback;
    if (callback) {
        MMKV::registerContentChangeHandler(myContentChangeHandler);
    } else {
        MMKV::unRegisterContentChangeHandler();
    }
}

MMKV_EXPORT void checkContentChanged(void *handle) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        kv->checkContentChanged();
    }
}

#endif // MMKV_DISABLE_FLUTTER
