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

#ifndef MMKV_CBRIDGE_H
#define MMKV_CBRIDGE_H

#ifndef MMKV_CBRIDGE_API
#    if defined(_WIN32) && defined(MMKV_CBRIDGE_IMPLEMENTATION)
#        define MMKV_CBRIDGE_API __declspec(dllexport)
#    else
#        define MMKV_CBRIDGE_API
#    endif
#endif

#ifdef __cplusplus
#    include <cstdint>
extern "C" {
#else
#    include <stdbool.h>
#    include <stdint.h>
#endif

/* ── Types ─────────────────────────────────────────────────────────── */

/* All filesystem paths use UTF-8, including on Windows. */

/* Opaque handle for an MMKV instance */
typedef void *MMKVHandle_t;

/* All-in-one creation config */
typedef struct {
    int32_t mode;                   /* MMKVMode bitmask */

    const char *cryptKey;           /* NULL = no encryption */
    bool aes256;                    /* use AES-256 key length */

    const char *rootPath;           /* UTF-8; NULL = default root */

    uint64_t expectedCapacity;      /* initial file size (0 = default) */

    int32_t enableKeyExpire;        /* <0 = leave default, 0 = false, >0 = true */
    uint32_t expiredInSeconds;      /* 0 = ExpireNever */

    bool enableCompareBeforeSet;

    int32_t recover;                /* <0 = leave default, >=0 = MMKVRecoverStrategic */

    uint32_t itemSizeLimit;         /* 0 = no limit */
} MMKVConfig_t;

/* Callback types */
typedef void (*mmkv_log_callback_t)(int32_t level, const char *file, int32_t line,
                                    const char *function, const char *message);
typedef int32_t (*mmkv_error_callback_t)(const char *mmapID, int32_t error);
typedef void (*mmkv_content_change_callback_t)(const char *mmapID);
typedef void (*mmkv_content_load_callback_t)(const char *mmapID);

/* All handler callbacks grouped in one struct */
typedef struct {
    mmkv_log_callback_t log;
    mmkv_error_callback_t error;
    mmkv_content_change_callback_t contentChange;
    mmkv_content_load_callback_t contentLoad;
} MMKVHandler_t;

/* ── Init & lifecycle ──────────────────────────────────────────────── */

MMKV_CBRIDGE_API void mmkv_initialize(const char *rootDir, int32_t logLevel);
MMKV_CBRIDGE_API void mmkv_initialize_with_handler(const char *rootDir, int32_t logLevel, MMKVHandler_t handler);
MMKV_CBRIDGE_API void mmkv_register_handler(MMKVHandler_t handler);
MMKV_CBRIDGE_API void mmkv_unregister_handler(void);
MMKV_CBRIDGE_API void mmkv_set_log_level(int32_t logLevel);
MMKV_CBRIDGE_API void mmkv_on_exit(void);

/* ── Instance creation & management ────────────────────────────────── */

/* Create the default MMKV instance with the given configuration. */
MMKV_CBRIDGE_API MMKVHandle_t mmkv_default(MMKVConfig_t config);
/* Create an MMKV instance with an unique ID and the given configuration. */
MMKV_CBRIDGE_API MMKVHandle_t mmkv_with_id(const char *mmapID, MMKVConfig_t config);
/* Returns internal pointer — valid for the lifetime of the instance. Do NOT free. */
MMKV_CBRIDGE_API const char *mmkv_mmap_id(MMKVHandle_t handle);
/* Permanently close an MMKV instance. All references backed by the same native
   instance become invalid immediately. This operation must not race with any
   other operation, and no reference may be used afterward. */
MMKV_CBRIDGE_API void mmkv_close(MMKVHandle_t handle);
/* Close an MMKV instance and clear the caller's handle.
   This is the preferred idempotent form for managed bindings. */
MMKV_CBRIDGE_API void mmkv_close_handle(MMKVHandle_t *handle);

/* ── Encode ────────────────────────────────────────────────────────── */

MMKV_CBRIDGE_API bool mmkv_encode_bool(MMKVHandle_t handle, const char *key, bool value);
MMKV_CBRIDGE_API bool mmkv_encode_bool_v2(MMKVHandle_t handle, const char *key, bool value, uint32_t expireDuration);
MMKV_CBRIDGE_API bool mmkv_encode_int32(MMKVHandle_t handle, const char *key, int32_t value);
MMKV_CBRIDGE_API bool mmkv_encode_int32_v2(MMKVHandle_t handle, const char *key, int32_t value, uint32_t expireDuration);
MMKV_CBRIDGE_API bool mmkv_encode_uint32(MMKVHandle_t handle, const char *key, uint32_t value);
MMKV_CBRIDGE_API bool mmkv_encode_uint32_v2(MMKVHandle_t handle, const char *key, uint32_t value, uint32_t expireDuration);
MMKV_CBRIDGE_API bool mmkv_encode_int64(MMKVHandle_t handle, const char *key, int64_t value);
MMKV_CBRIDGE_API bool mmkv_encode_int64_v2(MMKVHandle_t handle, const char *key, int64_t value, uint32_t expireDuration);
MMKV_CBRIDGE_API bool mmkv_encode_uint64(MMKVHandle_t handle, const char *key, uint64_t value);
MMKV_CBRIDGE_API bool mmkv_encode_uint64_v2(MMKVHandle_t handle, const char *key, uint64_t value, uint32_t expireDuration);
MMKV_CBRIDGE_API bool mmkv_encode_float(MMKVHandle_t handle, const char *key, float value);
MMKV_CBRIDGE_API bool mmkv_encode_float_v2(MMKVHandle_t handle, const char *key, float value, uint32_t expireDuration);
MMKV_CBRIDGE_API bool mmkv_encode_double(MMKVHandle_t handle, const char *key, double value);
MMKV_CBRIDGE_API bool mmkv_encode_double_v2(MMKVHandle_t handle, const char *key, double value, uint32_t expireDuration);
/* For strings: value is null-terminated. Pass NULL to remove the key. */
MMKV_CBRIDGE_API bool mmkv_encode_string(MMKVHandle_t handle, const char *key, const char *value);
MMKV_CBRIDGE_API bool mmkv_encode_string_v2(MMKVHandle_t handle, const char *key, const char *value, uint32_t expireDuration);
/* For bytes: value/length pair. A non-NULL pointer with length 0 stores an
   empty byte array. Pass NULL/0 to remove the key. */
MMKV_CBRIDGE_API bool mmkv_encode_bytes(MMKVHandle_t handle, const char *key, const void *value, int64_t length);
MMKV_CBRIDGE_API bool mmkv_encode_bytes_v2(MMKVHandle_t handle, const char *key, const void *value, int64_t length, uint32_t expireDuration);

/* ── Decode ────────────────────────────────────────────────────────── */

MMKV_CBRIDGE_API bool mmkv_decode_bool(MMKVHandle_t handle, const char *key, bool defaultValue);
MMKV_CBRIDGE_API int32_t mmkv_decode_int32(MMKVHandle_t handle, const char *key, int32_t defaultValue);
MMKV_CBRIDGE_API uint32_t mmkv_decode_uint32(MMKVHandle_t handle, const char *key, uint32_t defaultValue);
MMKV_CBRIDGE_API int64_t mmkv_decode_int64(MMKVHandle_t handle, const char *key, int64_t defaultValue);
MMKV_CBRIDGE_API uint64_t mmkv_decode_uint64(MMKVHandle_t handle, const char *key, uint64_t defaultValue);
MMKV_CBRIDGE_API float mmkv_decode_float(MMKVHandle_t handle, const char *key, float defaultValue);
MMKV_CBRIDGE_API double mmkv_decode_double(MMKVHandle_t handle, const char *key, double defaultValue);
/* Returns malloc'd string — caller must call mmkv_free(). Returns NULL if not found. */
MMKV_CBRIDGE_API char *mmkv_decode_string(MMKVHandle_t handle, const char *key);
/* Returns malloc'd bytes — caller must call mmkv_free(). *lengthPtr receives
   size. An empty stored value returns a non-NULL allocation with size 0.
   Returns NULL if not found. */
MMKV_CBRIDGE_API void *mmkv_decode_bytes(MMKVHandle_t handle, const char *key, uint64_t *lengthPtr);

/* ── Value inspection ──────────────────────────────────────────────── */

/* Return the size of the key's value.
   actualSize = false: the size consumption including protobuf encoding overhead.
   actualSize = true : the value's raw length without encoding overhead. */
MMKV_CBRIDGE_API uint64_t mmkv_get_value_size(MMKVHandle_t handle, const char *key, bool actualSize);
/* Write value directly into caller-provided buffer. Returns bytes written, or -1 on error. */
MMKV_CBRIDGE_API int32_t mmkv_write_value_to_buffer(MMKVHandle_t handle, const char *key, void *ptr, int32_t size);

/* ── Encryption ────────────────────────────────────────────────────── */

/* Transform plain text into encrypted text, or vice versa by passing NULL/empty cryptKey. */
MMKV_CBRIDGE_API bool mmkv_rekey(MMKVHandle_t handle, const char *cryptKey, bool aes256);
/* Returns malloc'd crypt key bytes — caller must call mmkv_free(). *lengthPtr receives size. */
MMKV_CBRIDGE_API void *mmkv_crypt_key(MMKVHandle_t handle, uint32_t *lengthPtr);
/* Reset the encryption key (will not encrypt or decrypt anything).
   Typically called after another process has reKey'd a multi-process instance. */
MMKV_CBRIDGE_API void mmkv_check_reset_crypt_key(MMKVHandle_t handle, const char *cryptKey, bool aes256);

/* ── Keys & data management ────────────────────────────────────────── */

/* Returns malloc'd array of malloc'd strings. *lengthPtr receives count.
   Caller must free each string and then the array with mmkv_free(). */
MMKV_CBRIDGE_API char **mmkv_all_keys(MMKVHandle_t handle, uint64_t *lengthPtr, bool filterExpire);
MMKV_CBRIDGE_API bool mmkv_contains_key(MMKVHandle_t handle, const char *key);
MMKV_CBRIDGE_API uint64_t mmkv_count(MMKVHandle_t handle, bool filterExpire);
MMKV_CBRIDGE_API uint64_t mmkv_total_size(MMKVHandle_t handle);
MMKV_CBRIDGE_API uint64_t mmkv_actual_size(MMKVHandle_t handle);

MMKV_CBRIDGE_API void mmkv_remove_value(MMKVHandle_t handle, const char *key);
MMKV_CBRIDGE_API void mmkv_remove_values(MMKVHandle_t handle, const char **keyArray, uint64_t count);
/* keepSpace = true: remove all keys but keep the file size unchanged (faster). */
MMKV_CBRIDGE_API void mmkv_clear_all(MMKVHandle_t handle, bool keepSpace);
MMKV_CBRIDGE_API uint64_t mmkv_import_from(MMKVHandle_t handle, MMKVHandle_t srcHandle);

/* ── Sync & memory ─────────────────────────────────────────────────── */

/* Save all mmap memory to file. syncFlag = true for sync, false for async. */
MMKV_CBRIDGE_API void mmkv_sync(MMKVHandle_t handle, bool syncFlag);
/* Clear the in-memory cache. keepSpace = true: keep file size for later use. */
MMKV_CBRIDGE_API void mmkv_clear_memory_cache(MMKVHandle_t handle, bool keepSpace);
/* Reclaim disk space after lots of deleting. */
MMKV_CBRIDGE_API void mmkv_trim(MMKVHandle_t handle);
/* Check if content changed by other process. */
MMKV_CBRIDGE_API void mmkv_check_content_changed(MMKVHandle_t handle);

/* ── Inter-process lock ────────────────────────────────────────────── */

/* Acquire exclusive inter-process lock. Blocks until acquired. */
MMKV_CBRIDGE_API void mmkv_lock(MMKVHandle_t handle);
/* Release the exclusive inter-process lock. */
MMKV_CBRIDGE_API void mmkv_unlock(MMKVHandle_t handle);
/* Try to acquire the lock without blocking. Returns true if acquired. */
MMKV_CBRIDGE_API bool mmkv_try_lock(MMKVHandle_t handle);

/* ── Backup / Restore (global) ─────────────────────────────────────── */

/* rootPath: the root dir of the source/destination MMKV. NULL = default root dir. */
MMKV_CBRIDGE_API bool mmkv_backup_one(const char *mmapID, const char *dstDir, const char *rootPath);
MMKV_CBRIDGE_API bool mmkv_restore_one(const char *mmapID, const char *srcDir, const char *rootPath);
MMKV_CBRIDGE_API uint64_t mmkv_backup_all(const char *dstDir, const char *rootPath);
MMKV_CBRIDGE_API uint64_t mmkv_restore_all(const char *srcDir, const char *rootPath);

/* ── Features ──────────────────────────────────────────────────────── */

MMKV_CBRIDGE_API bool mmkv_enable_auto_expire(MMKVHandle_t handle, uint32_t expireDuration);
MMKV_CBRIDGE_API bool mmkv_disable_auto_expire(MMKVHandle_t handle);
MMKV_CBRIDGE_API bool mmkv_enable_compare_before_set(MMKVHandle_t handle);
MMKV_CBRIDGE_API bool mmkv_disable_compare_before_set(MMKVHandle_t handle);

/* ── Instance property queries ─────────────────────────────────────── */

MMKV_CBRIDGE_API bool mmkv_is_multi_process(MMKVHandle_t handle);
MMKV_CBRIDGE_API bool mmkv_is_read_only(MMKVHandle_t handle);
MMKV_CBRIDGE_API bool mmkv_is_expiration_enabled(MMKVHandle_t handle);
MMKV_CBRIDGE_API bool mmkv_is_encryption_enabled(MMKVHandle_t handle);
MMKV_CBRIDGE_API bool mmkv_is_compare_before_set_enabled(MMKVHandle_t handle);

/* ── Static info ───────────────────────────────────────────────────── */

MMKV_CBRIDGE_API int32_t mmkv_page_size(void);
MMKV_CBRIDGE_API const char *mmkv_version(void);
/* Returns a borrowed UTF-8 pointer. Do NOT free. */
MMKV_CBRIDGE_API const char *mmkv_root_dir(void);

/* ── Static storage operations ─────────────────────────────────────── */

MMKV_CBRIDGE_API bool mmkv_remove_storage(const char *mmapID, const char *rootPath);
MMKV_CBRIDGE_API bool mmkv_check_exist(const char *mmapID, const char *rootPath);
/* Check whether the MMKV file is valid or not.
   Note: Don't use this to check existence; result is undefined on nonexistent files. */
MMKV_CBRIDGE_API bool mmkv_is_file_valid(const char *mmapID, const char *rootPath);

/* ── NameSpace ─────────────────────────────────────────────────────── */

/* Opaque handle for a NameSpace (custom root dir) */
typedef void *MMKVNameSpace_t;

/* Create a namespace with a custom root dir.
   Caller must call mmkv_namespace_free() when done. */
MMKV_CBRIDGE_API MMKVNameSpace_t mmkv_namespace(const char *rootDir);
/* Create the default namespace (global root dir).
   Caller must call mmkv_namespace_free() when done. */
MMKV_CBRIDGE_API MMKVNameSpace_t mmkv_default_namespace(void);
/* Free a namespace handle. */
MMKV_CBRIDGE_API void mmkv_namespace_free(MMKVNameSpace_t ns);
/* Get the UTF-8 root dir of a namespace. Returns a borrowed pointer — do NOT free. */
MMKV_CBRIDGE_API const char *mmkv_namespace_root_dir(MMKVNameSpace_t ns);

/* Create an MMKV instance within the namespace. */
MMKV_CBRIDGE_API MMKVHandle_t mmkv_namespace_mmkv_with_id(MMKVNameSpace_t ns, const char *mmapID, MMKVConfig_t config);
MMKV_CBRIDGE_API bool mmkv_namespace_backup_one(MMKVNameSpace_t ns, const char *mmapID, const char *dstDir);
MMKV_CBRIDGE_API bool mmkv_namespace_restore_one(MMKVNameSpace_t ns, const char *mmapID, const char *srcDir);
MMKV_CBRIDGE_API uint64_t mmkv_namespace_backup_all(MMKVNameSpace_t ns, const char *dstDir);
MMKV_CBRIDGE_API uint64_t mmkv_namespace_restore_all(MMKVNameSpace_t ns, const char *srcDir);
MMKV_CBRIDGE_API bool mmkv_namespace_is_file_valid(MMKVNameSpace_t ns, const char *mmapID);
MMKV_CBRIDGE_API bool mmkv_namespace_remove_storage(MMKVNameSpace_t ns, const char *mmapID);
MMKV_CBRIDGE_API bool mmkv_namespace_check_exist(MMKVNameSpace_t ns, const char *mmapID);

/* ── Memory management ─────────────────────────────────────────────── */

/* Free any heap pointer returned by mmkv_decode_string, mmkv_decode_bytes,
   mmkv_all_keys, mmkv_crypt_key, etc. */
MMKV_CBRIDGE_API void mmkv_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MMKV_CBRIDGE_H */
