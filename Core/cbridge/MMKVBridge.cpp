/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
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

#include "MMKV.h"
#include "MMKVBridge.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace mmkv;
using namespace std;

/* ── Visibility ────────────────────────────────────────────────────── */

#ifdef MMKV_EXPORT
#    undef MMKV_EXPORT
#endif
#define MMKV_EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))

/* ── Handler bridge ────────────────────────────────────────────────── */

class CMMKVHandler : public mmkv::MMKVHandler {
public:
    MMKVHandler_t m_callbacks = {};

    void mmkvLog(MMKVLogLevel level, const char *file, int line,
                 const char *function, const std::string &message) override {
        if (m_callbacks.log) {
            m_callbacks.log(static_cast<int32_t>(level), file, line, function, message.c_str());
        }
    }

    MMKVRecoverStrategic onMMKVCRCCheckFail(const std::string &mmapID) override {
        if (m_callbacks.error) {
            return static_cast<MMKVRecoverStrategic>(
                m_callbacks.error(mmapID.c_str(), static_cast<int32_t>(MMKVCRCCheckFail)));
        }
        return OnErrorDiscard;
    }

    MMKVRecoverStrategic onMMKVFileLengthError(const std::string &mmapID) override {
        if (m_callbacks.error) {
            return static_cast<MMKVRecoverStrategic>(
                m_callbacks.error(mmapID.c_str(), static_cast<int32_t>(MMKVFileLength)));
        }
        return OnErrorDiscard;
    }

    void onContentChangedByOuterProcess(const std::string &mmapID) override {
        if (m_callbacks.contentChange) {
            m_callbacks.contentChange(mmapID.c_str());
        }
    }

    void onMMKVContentLoadSuccessfully(const std::string &mmapID) override {
        if (m_callbacks.contentLoad) {
            m_callbacks.contentLoad(mmapID.c_str());
        }
    }
};

static CMMKVHandler g_cHandler;

/* ── Helper: build MMKVConfig from C struct ────────────────────────── */

static inline MMKV *kvFromHandle(void *handle) {
    return static_cast<MMKV *>(handle);
}

static MMKVConfig buildConfig(const MMKVConfig_t &cfg,
                              string &cryptStorage, string &pathStorage) {
    MMKVConfig config;
    config.mode = static_cast<MMKVMode>(cfg.mode);
#ifndef MMKV_DISABLE_CRYPT
    config.aes256 = cfg.aes256;
#endif
    config.expectedCapacity = cfg.expectedCapacity;
    if (cfg.enableKeyExpire >= 0) {
        config.enableKeyExpire = (cfg.enableKeyExpire != 0);
    }
    config.expiredInSeconds = cfg.expiredInSeconds;
    config.enableCompareBeforeSet = cfg.enableCompareBeforeSet;
    if (cfg.recover >= 0) {
        config.recover = static_cast<MMKVRecoverStrategic>(cfg.recover);
    }
    config.itemSizeLimit = cfg.itemSizeLimit;

#ifndef MMKV_DISABLE_CRYPT
    if (cfg.cryptKey && cfg.cryptKey[0]) {
        cryptStorage = cfg.cryptKey;
        config.cryptKey = &cryptStorage;
    }
#endif
    if (cfg.rootPath && cfg.rootPath[0]) {
        pathStorage = cfg.rootPath;
        config.rootPath = &pathStorage;
    }
    return config;
}

/* ── Init & lifecycle ──────────────────────────────────────────────── */

MMKV_EXPORT void mmkv_initialize(const char *rootDir, int32_t logLevel) {
    if (!rootDir) { return; }
    MMKV::initializeMMKV(string(rootDir), static_cast<MMKVLogLevel>(logLevel));
}

MMKV_EXPORT void mmkv_initialize_with_handler(const char *rootDir, int32_t logLevel, MMKVHandler_t handler) {
    if (!rootDir) { return; }
    g_cHandler.m_callbacks = handler;
    MMKV::initializeMMKV(string(rootDir), static_cast<MMKVLogLevel>(logLevel), &g_cHandler);
}

MMKV_EXPORT void mmkv_on_exit(void) {
    MMKV::onExit();
}

/* ── Instance management ───────────────────────────────────────────── */

MMKV_EXPORT MMKVHandle_t mmkv_default(MMKVConfig_t cfg) {
    string cryptStorage, pathStorage;
    auto config = buildConfig(cfg, cryptStorage, pathStorage);
    return MMKV::defaultMMKV(config);
}

MMKV_EXPORT MMKVHandle_t mmkv_with_id(const char *mmapID, MMKVConfig_t cfg) {
    if (!mmapID) { return nullptr; }
    string cryptStorage, pathStorage;
    auto config = buildConfig(cfg, cryptStorage, pathStorage);
    return MMKV::mmkvWithID(string(mmapID), config);
}

MMKV_EXPORT const char *mmkv_mmap_id(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) {
        return kv->mmapID().c_str();
    }
    return nullptr;
}

MMKV_EXPORT void mmkv_close(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->close(); }
}

/* ── Encode: bool ──────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_bool(MMKVHandle_t handle, const char *key, bool value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key); }
    return false;
}

MMKV_EXPORT bool mmkv_encode_bool_v2(MMKVHandle_t handle, const char *key, bool value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key, expireDuration); }
    return false;
}

/* ── Encode: int32 ─────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_int32(MMKVHandle_t handle, const char *key, int32_t value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key); }
    return false;
}

MMKV_EXPORT bool mmkv_encode_int32_v2(MMKVHandle_t handle, const char *key, int32_t value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key, expireDuration); }
    return false;
}

/* ── Encode: uint32 ────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_uint32(MMKVHandle_t handle, const char *key, uint32_t value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key); }
    return false;
}

MMKV_EXPORT bool mmkv_encode_uint32_v2(MMKVHandle_t handle, const char *key, uint32_t value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key, expireDuration); }
    return false;
}

/* ── Encode: int64 ─────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_int64(MMKVHandle_t handle, const char *key, int64_t value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key); }
    return false;
}

MMKV_EXPORT bool mmkv_encode_int64_v2(MMKVHandle_t handle, const char *key, int64_t value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key, expireDuration); }
    return false;
}

/* ── Encode: uint64 ────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_uint64(MMKVHandle_t handle, const char *key, uint64_t value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key); }
    return false;
}

MMKV_EXPORT bool mmkv_encode_uint64_v2(MMKVHandle_t handle, const char *key, uint64_t value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key, expireDuration); }
    return false;
}

/* ── Encode: float ─────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_float(MMKVHandle_t handle, const char *key, float value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key); }
    return false;
}

MMKV_EXPORT bool mmkv_encode_float_v2(MMKVHandle_t handle, const char *key, float value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key, expireDuration); }
    return false;
}

/* ── Encode: double ────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_double(MMKVHandle_t handle, const char *key, double value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key); }
    return false;
}

MMKV_EXPORT bool mmkv_encode_double_v2(MMKVHandle_t handle, const char *key, double value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->set(value, key, expireDuration); }
    return false;
}

/* ── Encode: string ────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_string(MMKVHandle_t handle, const char *key, const char *value) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) {
        if (value) {
            return kv->set(value, key);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

MMKV_EXPORT bool mmkv_encode_string_v2(MMKVHandle_t handle, const char *key, const char *value, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) {
        if (value) {
            return kv->set(value, key, expireDuration);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

/* ── Encode: bytes ─────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_encode_bytes(MMKVHandle_t handle, const char *key, const void *value, int64_t length) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) {
        if (value && length > 0) {
            auto buf = MMBuffer((void *) value, static_cast<size_t>(length), MMBufferNoCopy);
            return kv->set(buf, key);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

MMKV_EXPORT bool mmkv_encode_bytes_v2(MMKVHandle_t handle, const char *key, const void *value, int64_t length, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) {
        if (value && length > 0) {
            auto buf = MMBuffer((void *) value, static_cast<size_t>(length), MMBufferNoCopy);
            return kv->set(buf, key, expireDuration);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

/* ── Decode: primitives ────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_decode_bool(MMKVHandle_t handle, const char *key, bool defaultValue) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getBool(key, defaultValue); }
    return defaultValue;
}

MMKV_EXPORT int32_t mmkv_decode_int32(MMKVHandle_t handle, const char *key, int32_t defaultValue) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getInt32(key, defaultValue); }
    return defaultValue;
}

MMKV_EXPORT uint32_t mmkv_decode_uint32(MMKVHandle_t handle, const char *key, uint32_t defaultValue) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getUInt32(key, defaultValue); }
    return defaultValue;
}

MMKV_EXPORT int64_t mmkv_decode_int64(MMKVHandle_t handle, const char *key, int64_t defaultValue) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getInt64(key, defaultValue); }
    return defaultValue;
}

MMKV_EXPORT uint64_t mmkv_decode_uint64(MMKVHandle_t handle, const char *key, uint64_t defaultValue) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getUInt64(key, defaultValue); }
    return defaultValue;
}

MMKV_EXPORT float mmkv_decode_float(MMKVHandle_t handle, const char *key, float defaultValue) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getFloat(key, defaultValue); }
    return defaultValue;
}

MMKV_EXPORT double mmkv_decode_double(MMKVHandle_t handle, const char *key, double defaultValue) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getDouble(key, defaultValue); }
    return defaultValue;
}

/* ── Decode: string ────────────────────────────────────────────────── */

MMKV_EXPORT char *mmkv_decode_string(MMKVHandle_t handle, const char *key) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) {
        string result;
        if (kv->getString(key, result)) {
            return strdup(result.c_str());
        }
    }
    return nullptr;
}

/* ── Decode: bytes ─────────────────────────────────────────────────── */

MMKV_EXPORT void *mmkv_decode_bytes(MMKVHandle_t handle, const char *key, uint64_t *lengthPtr) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) {
        auto value = kv->getBytes(key);
        if (value.length() > 0) {
            if (value.isStoredOnStack()) {
                void *result = malloc(value.length());
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

/* ── Encryption ────────────────────────────────────────────────────── */

#ifndef MMKV_DISABLE_CRYPT

MMKV_EXPORT bool mmkv_rekey(MMKVHandle_t handle, const char *cryptKey, bool aes256) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) {
        if (cryptKey && cryptKey[0]) {
            return kv->reKey(string(cryptKey), aes256);
        } else {
            return kv->reKey(string(), aes256);
        }
    }
    return false;
}

MMKV_EXPORT void *mmkv_crypt_key(MMKVHandle_t handle, uint32_t *lengthPtr) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && lengthPtr) {
        auto key = kv->cryptKey();
        if (key.length() > 0) {
            void *ptr = malloc(key.length());
            if (ptr) {
                memcpy(ptr, key.data(), key.length());
                *lengthPtr = static_cast<uint32_t>(key.length());
                return ptr;
            }
        }
    }
    return nullptr;
}

MMKV_EXPORT void mmkv_check_reset_crypt_key(MMKVHandle_t handle, const char *cryptKey, bool aes256) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) {
        if (cryptKey && cryptKey[0]) {
            string key(cryptKey);
            kv->checkReSetCryptKey(&key, aes256);
        } else {
            kv->checkReSetCryptKey(nullptr, aes256);
        }
    }
}

#else

MMKV_EXPORT bool mmkv_rekey(MMKVHandle_t handle, const char *cryptKey, bool aes256) {
    return false;
}

MMKV_EXPORT void *mmkv_crypt_key(MMKVHandle_t handle, uint32_t *lengthPtr) {
    return nullptr;
}

MMKV_EXPORT void mmkv_check_reset_crypt_key(MMKVHandle_t handle, const char *cryptKey, bool aes256) {}

#endif /* MMKV_DISABLE_CRYPT */

/* ── Keys ──────────────────────────────────────────────────────────── */

MMKV_EXPORT char **mmkv_all_keys(MMKVHandle_t handle, uint64_t *lengthPtr, bool filterExpire) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) {
        auto keys = kv->allKeys(filterExpire);
        if (!keys.empty()) {
            char **arr = static_cast<char **>(calloc(keys.size(), sizeof(char *)));
            if (!arr) { return nullptr; }
            for (size_t i = 0; i < keys.size(); i++) {
                arr[i] = strdup(keys[i].c_str());
            }
            *lengthPtr = keys.size();
            return arr;
        }
    }
    return nullptr;
}

MMKV_EXPORT bool mmkv_contains_key(MMKVHandle_t handle, const char *key) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->containsKey(key); }
    return false;
}

MMKV_EXPORT uint64_t mmkv_count(MMKVHandle_t handle, bool filterExpire) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->count(filterExpire); }
    return 0;
}

MMKV_EXPORT uint64_t mmkv_total_size(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->totalSize(); }
    return 0;
}

MMKV_EXPORT uint64_t mmkv_actual_size(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->actualSize(); }
    return 0;
}

MMKV_EXPORT void mmkv_remove_value(MMKVHandle_t handle, const char *key) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { kv->removeValueForKey(key); }
}

MMKV_EXPORT void mmkv_remove_values(MMKVHandle_t handle, const char **keyArray, uint64_t count) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && keyArray && count > 0) {
        vector<string> arrKeys;
        arrKeys.reserve(count);
        for (uint64_t i = 0; i < count; i++) {
            if (keyArray[i]) {
                arrKeys.emplace_back(keyArray[i]);
            }
        }
        if (!arrKeys.empty()) {
            kv->removeValuesForKeys(arrKeys);
        }
    }
}

MMKV_EXPORT void mmkv_clear_all(MMKVHandle_t handle, bool keepSpace) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->clearAll(keepSpace); }
}

MMKV_EXPORT uint64_t mmkv_import_from(MMKVHandle_t handle, MMKVHandle_t srcHandle) {
    MMKV *kv = kvFromHandle(handle);
    MMKV *src = kvFromHandle(srcHandle);
    if (kv && src) { return kv->importFrom(src); }
    return 0;
}

/* ── Sync ──────────────────────────────────────────────────────────── */

MMKV_EXPORT void mmkv_sync(MMKVHandle_t handle, bool syncFlag) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->sync(static_cast<SyncFlag>(syncFlag)); }
}

MMKV_EXPORT void mmkv_clear_memory_cache(MMKVHandle_t handle, bool keepSpace) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->clearMemoryCache(keepSpace); }
}

MMKV_EXPORT void mmkv_trim(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->trim(); }
}

/* ── Lock ──────────────────────────────────────────────────────────── */

MMKV_EXPORT void mmkv_lock(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->lock(); }
}

MMKV_EXPORT void mmkv_unlock(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->unlock(); }
}

MMKV_EXPORT bool mmkv_try_lock(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->try_lock(); }
    return false;
}

/* ── Backup / Restore ──────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_backup_one(const char *mmapID, const char *dstDir, const char *rootPath) {
    if (!mmapID || !dstDir) { return false; }
    if (rootPath) {
        string path(rootPath);
        return MMKV::backupOneToDirectory(string(mmapID), string(dstDir), &path);
    }
    return MMKV::backupOneToDirectory(string(mmapID), string(dstDir), nullptr);
}

MMKV_EXPORT bool mmkv_restore_one(const char *mmapID, const char *srcDir, const char *rootPath) {
    if (!mmapID || !srcDir) { return false; }
    if (rootPath) {
        string path(rootPath);
        return MMKV::restoreOneFromDirectory(string(mmapID), string(srcDir), &path);
    }
    return MMKV::restoreOneFromDirectory(string(mmapID), string(srcDir), nullptr);
}

MMKV_EXPORT uint64_t mmkv_backup_all(const char *dstDir, const char *rootPath) {
    if (!dstDir) { return 0; }
    if (rootPath) {
        string path(rootPath);
        return MMKV::backupAllToDirectory(string(dstDir), &path);
    }
    return MMKV::backupAllToDirectory(string(dstDir), nullptr);
}

MMKV_EXPORT uint64_t mmkv_restore_all(const char *srcDir, const char *rootPath) {
    if (!srcDir) { return 0; }
    if (rootPath) {
        string path(rootPath);
        return MMKV::restoreAllFromDirectory(string(srcDir), &path);
    }
    return MMKV::restoreAllFromDirectory(string(srcDir), nullptr);
}

/* ── Features ──────────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_enable_auto_expire(MMKVHandle_t handle, uint32_t expireDuration) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->enableAutoKeyExpire(expireDuration); }
    return false;
}

MMKV_EXPORT bool mmkv_disable_auto_expire(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->disableAutoKeyExpire(); }
    return false;
}

MMKV_EXPORT bool mmkv_enable_compare_before_set(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->enableCompareBeforeSet(); }
    return false;
}

MMKV_EXPORT bool mmkv_disable_compare_before_set(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->disableCompareBeforeSet(); }
    return false;
}

MMKV_EXPORT bool mmkv_is_expiration_enabled(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->isExpirationEnabled(); }
    return false;
}

MMKV_EXPORT bool mmkv_is_encryption_enabled(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->isEncryptionEnabled(); }
    return false;
}

MMKV_EXPORT bool mmkv_is_compare_before_set_enabled(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->isCompareBeforeSetEnabled(); }
    return false;
}

/* ── Value inspection ──────────────────────────────────────────────── */

MMKV_EXPORT uint64_t mmkv_get_value_size(MMKVHandle_t handle, const char *key, bool actualSize) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key) { return kv->getValueSize(key, actualSize); }
    return 0;
}

MMKV_EXPORT int32_t mmkv_write_value_to_buffer(MMKVHandle_t handle, const char *key, void *ptr, int32_t size) {
    MMKV *kv = kvFromHandle(handle);
    if (kv && key && ptr && size > 0) { return kv->writeValueToBuffer(key, ptr, size); }
    return -1;
}

/* ── Info ──────────────────────────────────────────────────────────── */

MMKV_EXPORT int32_t mmkv_page_size(void) {
    return static_cast<int32_t>(DEFAULT_MMAP_SIZE);
}

MMKV_EXPORT const char *mmkv_version(void) {
    return MMKV_VERSION;
}

MMKV_EXPORT const char *mmkv_root_dir(void) {
    return MMKV::getRootDir().c_str();
}

/* ── Storage ───────────────────────────────────────────────────────── */

MMKV_EXPORT bool mmkv_remove_storage(const char *mmapID, const char *rootPath) {
    if (!mmapID) { return false; }
    if (rootPath) {
        string path(rootPath);
        return MMKV::removeStorage(string(mmapID), &path);
    }
    return MMKV::removeStorage(string(mmapID), nullptr);
}

MMKV_EXPORT bool mmkv_check_exist(const char *mmapID, const char *rootPath) {
    if (!mmapID) { return false; }
    if (rootPath) {
        string path(rootPath);
        return MMKV::checkExist(string(mmapID), &path);
    }
    return MMKV::checkExist(string(mmapID), nullptr);
}

MMKV_EXPORT bool mmkv_is_multi_process(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->isMultiProcess(); }
    return false;
}

MMKV_EXPORT bool mmkv_is_read_only(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { return kv->isReadOnly(); }
    return false;
}

/* ── File validation & content change ──────────────────────────────── */

MMKV_EXPORT bool mmkv_is_file_valid(const char *mmapID, const char *rootPath) {
    if (!mmapID) { return false; }
    if (rootPath) {
        string path(rootPath);
        return MMKV::isFileValid(string(mmapID), &path);
    }
    return MMKV::isFileValid(string(mmapID), nullptr);
}

MMKV_EXPORT void mmkv_check_content_changed(MMKVHandle_t handle) {
    MMKV *kv = kvFromHandle(handle);
    if (kv) { kv->checkContentChanged(); }
}

/* ── NameSpace ─────────────────────────────────────────────────────── */

static inline mmkv::NameSpace *nsFromHandle(void *handle) {
    return static_cast<mmkv::NameSpace *>(handle);
}

MMKV_EXPORT MMKVNameSpace_t mmkv_namespace(const char *rootDir) {
    if (!rootDir) { return nullptr; }
    // NameSpace is a lightweight POD-like facade; we heap-allocate to return as opaque handle.
    auto *ns = new mmkv::NameSpace(MMKV::nameSpace(string(rootDir)));
    return ns;
}

MMKV_EXPORT MMKVNameSpace_t mmkv_default_namespace(void) {
    auto *ns = new mmkv::NameSpace(MMKV::defaultNameSpace());
    return ns;
}

MMKV_EXPORT void mmkv_namespace_free(MMKVNameSpace_t handle) {
    auto *ns = nsFromHandle(handle);
    delete ns;
}

MMKV_EXPORT const char *mmkv_namespace_root_dir(MMKVNameSpace_t handle) {
    auto *ns = nsFromHandle(handle);
    if (ns) { return ns->getRootDir().c_str(); }
    return nullptr;
}

MMKV_EXPORT MMKVHandle_t mmkv_namespace_mmkv_with_id(MMKVNameSpace_t handle, const char *mmapID, MMKVConfig_t cfg) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !mmapID) { return nullptr; }
    string cryptStorage, pathStorage;
    auto config = buildConfig(cfg, cryptStorage, pathStorage);
    return ns->mmkvWithID(string(mmapID), config);
}

MMKV_EXPORT bool mmkv_namespace_backup_one(MMKVNameSpace_t handle, const char *mmapID, const char *dstDir) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !mmapID || !dstDir) { return false; }
    return ns->backupOneToDirectory(string(mmapID), string(dstDir));
}

MMKV_EXPORT bool mmkv_namespace_restore_one(MMKVNameSpace_t handle, const char *mmapID, const char *srcDir) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !mmapID || !srcDir) { return false; }
    return ns->restoreOneFromDirectory(string(mmapID), string(srcDir));
}

MMKV_EXPORT uint64_t mmkv_namespace_backup_all(MMKVNameSpace_t handle, const char *dstDir) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !dstDir) { return 0; }
    return ns->backupAllToDirectory(string(dstDir));
}

MMKV_EXPORT uint64_t mmkv_namespace_restore_all(MMKVNameSpace_t handle, const char *srcDir) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !srcDir) { return 0; }
    return ns->restoreAllFromDirectory(string(srcDir));
}

MMKV_EXPORT bool mmkv_namespace_is_file_valid(MMKVNameSpace_t handle, const char *mmapID) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !mmapID) { return false; }
    return ns->isFileValid(string(mmapID));
}

MMKV_EXPORT bool mmkv_namespace_remove_storage(MMKVNameSpace_t handle, const char *mmapID) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !mmapID) { return false; }
    return ns->removeStorage(string(mmapID));
}

MMKV_EXPORT bool mmkv_namespace_check_exist(MMKVNameSpace_t handle, const char *mmapID) {
    auto *ns = nsFromHandle(handle);
    if (!ns || !mmapID) { return false; }
    return ns->checkExist(string(mmapID));
}

/* ── Memory management ─────────────────────────────────────────────── */

MMKV_EXPORT void mmkv_free(void *ptr) {
    free(ptr);
}
