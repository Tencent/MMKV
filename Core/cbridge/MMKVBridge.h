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

#ifdef __cplusplus
#    include <cstdint>
extern "C" {
#else
#    include <stdbool.h>
#    include <stdint.h>
#endif

/* ── Types ─────────────────────────────────────────────────────────── */

/* Opaque handle returned by mmkv_default / mmkv_with_id */
typedef void *MMKVHandle_t;

/* Creation config — mirrors MMKVCreationConfig_t but uses const char* */
typedef struct {
    int32_t mode;                   /* MMKVMode bitmask */

    const char *cryptKey;           /* NULL = no encryption */
    bool aes256;                    /* use AES-256 key length */

    const char *rootPath;           /* NULL = default root */

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

void mmkv_initialize(const char *rootDir, int32_t logLevel);
void mmkv_initialize_with_handler(const char *rootDir, int32_t logLevel, MMKVHandler_t handler);
void mmkv_on_exit(void);

/* ── Instance management ───────────────────────────────────────────── */

MMKVHandle_t mmkv_default(MMKVConfig_t config);
MMKVHandle_t mmkv_with_id(const char *mmapID, MMKVConfig_t config);
/* Returns internal pointer — valid for the lifetime of the instance. Do NOT free. */
const char *mmkv_mmap_id(MMKVHandle_t handle);
void mmkv_close(MMKVHandle_t handle);

/* ── Encode ────────────────────────────────────────────────────────── */

bool mmkv_encode_bool(MMKVHandle_t handle, const char *key, bool value);
bool mmkv_encode_bool_v2(MMKVHandle_t handle, const char *key, bool value, uint32_t expireDuration);
bool mmkv_encode_int32(MMKVHandle_t handle, const char *key, int32_t value);
bool mmkv_encode_int32_v2(MMKVHandle_t handle, const char *key, int32_t value, uint32_t expireDuration);
bool mmkv_encode_uint32(MMKVHandle_t handle, const char *key, uint32_t value);
bool mmkv_encode_uint32_v2(MMKVHandle_t handle, const char *key, uint32_t value, uint32_t expireDuration);
bool mmkv_encode_int64(MMKVHandle_t handle, const char *key, int64_t value);
bool mmkv_encode_int64_v2(MMKVHandle_t handle, const char *key, int64_t value, uint32_t expireDuration);
bool mmkv_encode_uint64(MMKVHandle_t handle, const char *key, uint64_t value);
bool mmkv_encode_uint64_v2(MMKVHandle_t handle, const char *key, uint64_t value, uint32_t expireDuration);
bool mmkv_encode_float(MMKVHandle_t handle, const char *key, float value);
bool mmkv_encode_float_v2(MMKVHandle_t handle, const char *key, float value, uint32_t expireDuration);
bool mmkv_encode_double(MMKVHandle_t handle, const char *key, double value);
bool mmkv_encode_double_v2(MMKVHandle_t handle, const char *key, double value, uint32_t expireDuration);
/* For strings: value is null-terminated. Pass NULL to remove the key. */
bool mmkv_encode_string(MMKVHandle_t handle, const char *key, const char *value);
bool mmkv_encode_string_v2(MMKVHandle_t handle, const char *key, const char *value, uint32_t expireDuration);
/* For bytes: value/length pair. Pass NULL/0 to remove the key. */
bool mmkv_encode_bytes(MMKVHandle_t handle, const char *key, const void *value, int64_t length);
bool mmkv_encode_bytes_v2(MMKVHandle_t handle, const char *key, const void *value, int64_t length, uint32_t expireDuration);

/* ── Decode ────────────────────────────────────────────────────────── */

bool mmkv_decode_bool(MMKVHandle_t handle, const char *key, bool defaultValue);
int32_t mmkv_decode_int32(MMKVHandle_t handle, const char *key, int32_t defaultValue);
uint32_t mmkv_decode_uint32(MMKVHandle_t handle, const char *key, uint32_t defaultValue);
int64_t mmkv_decode_int64(MMKVHandle_t handle, const char *key, int64_t defaultValue);
uint64_t mmkv_decode_uint64(MMKVHandle_t handle, const char *key, uint64_t defaultValue);
float mmkv_decode_float(MMKVHandle_t handle, const char *key, float defaultValue);
double mmkv_decode_double(MMKVHandle_t handle, const char *key, double defaultValue);
/* Returns malloc'd string — caller must call mmkv_free(). Returns NULL if not found. */
char *mmkv_decode_string(MMKVHandle_t handle, const char *key);
/* Returns malloc'd bytes — caller must call mmkv_free(). *lengthPtr receives size. Returns NULL if not found. */
void *mmkv_decode_bytes(MMKVHandle_t handle, const char *key, uint64_t *lengthPtr);

/* ── Encryption ────────────────────────────────────────────────────── */

bool mmkv_rekey(MMKVHandle_t handle, const char *cryptKey, bool aes256);
/* Returns malloc'd crypt key bytes — caller must call mmkv_free(). *lengthPtr receives size. */
void *mmkv_crypt_key(MMKVHandle_t handle, uint32_t *lengthPtr);
void mmkv_check_reset_crypt_key(MMKVHandle_t handle, const char *cryptKey, bool aes256);

/* ── Keys ──────────────────────────────────────────────────────────── */

/* Returns malloc'd array of malloc'd strings. *lengthPtr receives count.
   Caller must free each string and then the array with mmkv_free(). */
char **mmkv_all_keys(MMKVHandle_t handle, uint64_t *lengthPtr, bool filterExpire);
bool mmkv_contains_key(MMKVHandle_t handle, const char *key);
uint64_t mmkv_count(MMKVHandle_t handle, bool filterExpire);
uint64_t mmkv_total_size(MMKVHandle_t handle);
uint64_t mmkv_actual_size(MMKVHandle_t handle);

void mmkv_remove_value(MMKVHandle_t handle, const char *key);
void mmkv_remove_values(MMKVHandle_t handle, const char **keyArray, uint64_t count);
void mmkv_clear_all(MMKVHandle_t handle, bool keepSpace);
uint64_t mmkv_import_from(MMKVHandle_t handle, MMKVHandle_t srcHandle);

/* ── Sync ──────────────────────────────────────────────────────────── */

void mmkv_sync(MMKVHandle_t handle, bool sync);
void mmkv_clear_memory_cache(MMKVHandle_t handle);
void mmkv_trim(MMKVHandle_t handle);

/* ── Backup / Restore ──────────────────────────────────────────────── */

bool mmkv_backup_one(const char *mmapID, const char *dstDir, const char *srcDir);
bool mmkv_restore_one(const char *mmapID, const char *srcDir, const char *dstDir);
uint64_t mmkv_backup_all(const char *dstDir, const char *srcDir);
uint64_t mmkv_restore_all(const char *srcDir, const char *dstDir);

/* ── Features ──────────────────────────────────────────────────────── */

bool mmkv_enable_auto_expire(MMKVHandle_t handle, uint32_t expireDuration);
bool mmkv_disable_auto_expire(MMKVHandle_t handle);
bool mmkv_enable_compare_before_set(MMKVHandle_t handle);
bool mmkv_disable_compare_before_set(MMKVHandle_t handle);

/* ── Info ──────────────────────────────────────────────────────────── */

int32_t mmkv_page_size(void);
const char *mmkv_version(void);
const char *mmkv_root_dir(void);

/* ── Storage ───────────────────────────────────────────────────────── */

bool mmkv_remove_storage(const char *mmapID, const char *rootPath);
bool mmkv_check_exist(const char *mmapID, const char *rootPath);
bool mmkv_is_multi_process(MMKVHandle_t handle);
bool mmkv_is_read_only(MMKVHandle_t handle);

/* ── Memory management ─────────────────────────────────────────────── */

/* Free any heap pointer returned by mmkv_decode_string, mmkv_decode_bytes,
   mmkv_all_keys, mmkv_crypt_key, etc. */
void mmkv_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MMKV_CBRIDGE_H */
