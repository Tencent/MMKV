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

#ifndef CGO

#    include "MMKVPredef.h"

#    include "MMKV.h"
#    include "golang-bridge.h"
#    include <stdint.h>
#    include <string>
#    include <cstring>

using namespace mmkv;
using namespace std;

#    define MMKV_EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))

void cLogHandler(MMKVLogLevel level, const char *file, int line, const char *function, const std::string &message);

MMKV_EXPORT void mmkvInitialize(GoStringWrap rootDir, int32_t logLevel, bool redirect) {
    if (!rootDir.ptr) {
        return;
    }
    mmkv::LogHandler handler = redirect ? cLogHandler : nullptr;
    MMKV::initializeMMKV(string(rootDir.ptr, rootDir.length), (MMKVLogLevel) logLevel, handler);
}

MMKV_EXPORT void onExit() {
    MMKV::onExit();
}

MMKV_EXPORT void *getMMKVWithID(GoStringWrap mmapID, int32_t mode, GoStringWrap cryptKey, 
                                GoStringWrap rootPath, uint64_t expectedCapacity) {
    MMKV *kv = nullptr;
    if (!mmapID.ptr) {
        return kv;
    }
    auto str = string(mmapID.ptr, mmapID.length);

    bool done = false;
    if (cryptKey.ptr) {
        auto crypt = string(cryptKey.ptr, cryptKey.length);
        if (crypt.length() > 0) {
            if (rootPath.ptr) {
                auto path = string(rootPath.ptr, rootPath.length);
                kv = MMKV::mmkvWithID(str, (MMKVMode) mode, &crypt, &path, expectedCapacity);
            } else {
                kv = MMKV::mmkvWithID(str, (MMKVMode) mode, &crypt, nullptr, expectedCapacity);
            }
            done = true;
        }
    }
    if (!done) {
        if (rootPath.ptr) {
            auto path = string(rootPath.ptr, rootPath.length);
            kv = MMKV::mmkvWithID(str, (MMKVMode) mode, nullptr, &path, expectedCapacity);
        } else {
            kv = MMKV::mmkvWithID(str, (MMKVMode) mode, nullptr, nullptr, expectedCapacity);
        }
    }

    return kv;
}

MMKV_EXPORT void *getDefaultMMKV(int32_t mode, GoStringWrap cryptKey) {
    MMKV *kv = nullptr;

    if (cryptKey.ptr) {
        auto crypt = string(cryptKey.ptr, cryptKey.length);
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

MMKV_EXPORT bool encodeBool(void *handle, GoStringWrap oKey, bool value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((bool) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeBool_v2(void *handle, GoStringWrap oKey, bool value, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((bool) value, key, expireDuration);
    }
    return false;
}

MMKV_EXPORT bool decodeBool(void *handle, GoStringWrap oKey, bool defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->getBool(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeInt32(void *handle, GoStringWrap oKey, int32_t value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((int32_t) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeInt32_v2(void *handle, GoStringWrap oKey, int32_t value, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((int32_t) value, key, expireDuration);
    }
    return false;
}

MMKV_EXPORT int32_t decodeInt32(void *handle, GoStringWrap oKey, int32_t defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->getInt32(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeUInt32(void *handle, GoStringWrap oKey, uint32_t value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set(value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeUInt32_v2(void *handle, GoStringWrap oKey, uint32_t value, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set(value, key, expireDuration);
    }
    return false;
}

MMKV_EXPORT uint32_t decodeUInt32(void *handle, GoStringWrap oKey, uint32_t defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->getUInt32(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeInt64(void *handle, GoStringWrap oKey, int64_t value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((int64_t) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeInt64_v2(void *handle, GoStringWrap oKey, int64_t value, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((int64_t) value, key, expireDuration);
    }
    return false;
}

MMKV_EXPORT int64_t decodeInt64(void *handle, GoStringWrap oKey, int64_t defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->getInt64(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeUInt64(void *handle, GoStringWrap oKey, uint64_t value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set(value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeUInt64_v2(void *handle, GoStringWrap oKey, uint64_t value, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set(value, key, expireDuration);
    }
    return false;
}

MMKV_EXPORT uint64_t decodeUInt64(void *handle, GoStringWrap oKey, uint64_t defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->getUInt64(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeFloat(void *handle, GoStringWrap oKey, float value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((float) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeFloat_v2(void *handle, GoStringWrap oKey, float value, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((float) value, key, expireDuration);
    }
    return false;
}

MMKV_EXPORT float decodeFloat(void *handle, GoStringWrap oKey, float defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->getFloat(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeDouble(void *handle, GoStringWrap oKey, double value) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((double) value, key);
    }
    return false;
}

MMKV_EXPORT bool encodeDouble_v2(void *handle, GoStringWrap oKey, double value, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->set((double) value, key, expireDuration);
    }
    return false;
}

MMKV_EXPORT double decodeDouble(void *handle, GoStringWrap oKey, double defaultValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        return kv->getDouble(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT bool encodeBytes(void *handle, GoStringWrap oKey, GoStringWrap oValue) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        if (oValue.ptr) {
            auto value = MMBuffer((void *) oValue.ptr, oValue.length, MMBufferNoCopy);
            return kv->set(value, key);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

MMKV_EXPORT bool encodeBytes_v2(void *handle, GoStringWrap oKey, GoStringWrap oValue, uint32_t expireDuration) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        if (oValue.ptr) {
            auto value = MMBuffer((void *) oValue.ptr, oValue.length, MMBufferNoCopy);
            return kv->set(value, key, expireDuration);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

MMKV_EXPORT void *decodeBytes(void *handle, GoStringWrap oKey, uint64_t *lengthPtr) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        auto value = kv->getBytes(key);
        if (value.length() > 0) {
            if (value.isStoredOnStack()) {
                auto result = malloc(value.length());
                if (result) {
                    memcpy(result, value.getPtr(), value.length());
                    *lengthPtr = value.length();
                }
                return result;
            } else {
                void *result = value.getPtr();
                *lengthPtr = value.length();
                value.detach();
                return result;
            }
        }
    }
    return nullptr;
}

#    ifndef MMKV_DISABLE_CRYPT

MMKV_EXPORT bool reKey(void *handle, GoStringWrap oKey) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        if (oKey.ptr && oKey.length > 0) {
            string key(oKey.ptr, oKey.length);
            return kv->reKey(key);
        } else {
            return kv->reKey(string());
        }
    }
    return false;
}

MMKV_EXPORT void *cryptKey(void *handle, uint32_t *lengthPtr) {
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

MMKV_EXPORT void checkReSetCryptKey(void *handle, GoStringWrap oKey) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        if (oKey.ptr && oKey.length > 0) {
            string key(oKey.ptr, oKey.length);
            kv->checkReSetCryptKey(&key);
        } else {
            kv->checkReSetCryptKey(nullptr);
        }
    }
}

#    else // fix cgo cannot find function reference.

MMKV_EXPORT bool reKey(void *handle, GoStringWrap oKey) {
    return false;
}

MMKV_EXPORT void *cryptKey(void *handle, uint32_t *lengthPtr) {
    return nullptr;
}

MMKV_EXPORT void checkReSetCryptKey(void *handle, GoStringWrap oKey) {}

#    endif // MMKV_DISABLE_CRYPT

MMKV_EXPORT GoStringWrap *allKeys(void *handle, uint64_t *lengthPtr, bool filterExpire) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv) {
        auto keys = kv->allKeys(filterExpire);
        if (!keys.empty()) {
            auto keyArray = (GoStringWrap *) calloc(keys.size(), sizeof(GoStringWrap));
            if (!keyArray) {
                return nullptr;
            }

            for (size_t index = 0; index < keys.size(); index++) {
                auto &key = keys[index];
                auto &stringWrap = keyArray[index];
                stringWrap.length = static_cast<uint32_t>(key.length());
                stringWrap.ptr = (char *) malloc(key.length());
                if (stringWrap.ptr) {
                    memcpy((void *) stringWrap.ptr, key.data(), key.length());
                }
            }
            *lengthPtr = keys.size();
            return keyArray;
        }
    }
    return nullptr;
}

MMKV_EXPORT bool containsKey(void *handle, GoStringWrap oKey) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
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

MMKV_EXPORT void removeValueForKey(void *handle, GoStringWrap oKey) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && oKey.ptr) {
        auto key = string(oKey.ptr, oKey.length);
        kv->removeValueForKey(key);
    }
}

MMKV_EXPORT void removeValuesForKeys(void *handle, GoStringWrap *keyArray, uint64_t count) {
    MMKV *kv = static_cast<MMKV *>(handle);
    if (kv && keyArray && count > 0) {
        vector<string> arrKeys;
        arrKeys.reserve(count);
        for (uint64_t index = 0; index < count; index++) {
            auto &stringWrap = keyArray[index];
            if (stringWrap.ptr && stringWrap.length > 0) {
                arrKeys.emplace_back(stringWrap.ptr, stringWrap.length);
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

MMKV_EXPORT bool backupOneToDirectory(GoStringWrap_t mmapID, GoStringWrap_t dstDir, GoStringWrap_t srcDir) {
    if (!mmapID.ptr || !dstDir.ptr) {
        return false;
    }
    auto id = string(mmapID.ptr, mmapID.length);
    auto dst = string(dstDir.ptr, dstDir.length);
    if (srcDir.ptr) {
        auto src = string(srcDir.ptr, srcDir.length);
        return MMKV::backupOneToDirectory(id, dst, &src);
    }
    return MMKV::backupOneToDirectory(id, dst, nullptr);
}

MMKV_EXPORT bool restoreOneFromDirectory(GoStringWrap_t mmapID, GoStringWrap_t srcDir, GoStringWrap_t dstDir) {
    if (!mmapID.ptr || !srcDir.ptr) {
        return false;
    }
    auto id = string(mmapID.ptr, mmapID.length);
    auto src = string(srcDir.ptr, srcDir.length);
    if (dstDir.ptr) {
        auto dst = string(dstDir.ptr, dstDir.length);
        return MMKV::restoreOneFromDirectory(id, src, &dst);
    }
    return MMKV::restoreOneFromDirectory(id, src, nullptr);
}

MMKV_EXPORT uint64_t backupAllToDirectory(GoStringWrap_t dstDir, GoStringWrap_t srcDir) {
    if (!dstDir.ptr) {
        return 0;
    }

    auto dst = string(dstDir.ptr, dstDir.length);
    if (srcDir.ptr) {
        auto src = string(srcDir.ptr, srcDir.length);
        return MMKV::backupAllToDirectory(dst, &src);
    }
    return MMKV::backupAllToDirectory(dst, nullptr);
}

MMKV_EXPORT uint64_t restoreAllFromDirectory(GoStringWrap_t srcDir, GoStringWrap_t dstDir) {
    if (!srcDir.ptr) {
        return 0;
    }

    auto src = string(srcDir.ptr, srcDir.length);
    if (dstDir.ptr) {
        auto dst = string(dstDir.ptr, dstDir.length);
        return MMKV::restoreAllFromDirectory(src, &dst);
    }
    return MMKV::restoreAllFromDirectory(src, nullptr);
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

extern "C" void myLogHandler(int64_t level, GoStringWrap file, int64_t line, GoStringWrap function, GoStringWrap message);

void cLogHandler(MMKVLogLevel level, const char *file, int line, const char *function, const std::string &message) {
    GoStringWrap oFile { file, static_cast<int64_t>(strlen(file)) };
    GoStringWrap oFunction { function, static_cast<int64_t>(strlen(function)) };
    GoStringWrap oMessage { message.data(), static_cast<int64_t>(message.length()) };

    myLogHandler(level, oFile, line, oFunction, oMessage);
}

void setWantsLogRedirect(bool redirect) {
    if (redirect) {
        MMKV::registerLogHandler(&cLogHandler);
    } else {
        MMKV::unRegisterLogHandler();
    }
}

extern "C" int64_t myErrorHandler(GoStringWrap mmapID, int64_t error);

static MMKVRecoverStrategic cErrorHandler(const std::string &mmapID, MMKVErrorType errorType) {
    GoStringWrap oID { mmapID.data(), static_cast<int64_t>(mmapID.length()) };

    return static_cast<MMKVRecoverStrategic>(myErrorHandler(oID, static_cast<int64_t>(errorType)));
}

void setWantsErrorHandle(bool errorHandle) {
    if (errorHandle) {
        MMKV::registerErrorHandler(&cErrorHandler);
    } else {
        MMKV::unRegisterErrorHandler();
    }
}

extern "C" void myContentChangeHandler(GoStringWrap mmapID);

static void cContentChangeHandler(const std::string &mmapID) {
    GoStringWrap oID { mmapID.data(), static_cast<int64_t>(mmapID.length()) };

    myContentChangeHandler(oID);
}

void setWantsContentChangeHandle(bool errorHandle) {
    if (errorHandle) {
        MMKV::registerContentChangeHandler(&cContentChangeHandler);
    } else {
        MMKV::unRegisterContentChangeHandler();
    }
}

MMKV_EXPORT bool removeStorage(GoStringWrap_t mmapID, GoStringWrap_t rootPath) {
    if (!mmapID.ptr) {
        return false;
    }
    auto id = string(mmapID.ptr, mmapID.length);
    if (rootPath.ptr) {
        auto path = string(rootPath.ptr, rootPath.length);
        return MMKV::removeStorage(id, &path);
    }
    return MMKV::removeStorage(id, nullptr);
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

#endif // CGO
