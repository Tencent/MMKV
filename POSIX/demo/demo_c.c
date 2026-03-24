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

#include <MMKVBridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <float.h>
#include <limits.h>
#include <math.h>

/* ── Log callback ──────────────────────────────────────────────────── */

static void myLogHandler(int32_t level, const char *file, int32_t line,
                         const char *function, const char *message) {
    const char *levelDesc = "?";
    switch (level) {
        case 0: levelDesc = "Debug"; break;
        case 1: levelDesc = "Info"; break;
        case 2: levelDesc = "Warning"; break;
        case 3: levelDesc = "Error"; break;
        default: break;
    }
    printf("[%s] <%s:%d::%s> %s\n", levelDesc, file, line, function, message);
}

/* ── Error callback ────────────────────────────────────────────────── */

static int32_t myErrorHandler(const char *mmapID, int32_t error) {
    printf("Error on [%s], error type: %d\n", mmapID, error);
    /* OnErrorRecover = 1 */
    return 1;
}

/* ── Functional test mirroring demo.cpp ────────────────────────────── */

static void functionalTest(MMKVHandle_t kv) {
    printf("\n=== Functional Test (C Bridge) ===\n");

    /* bool */
    mmkv_encode_bool(kv, "bool", true);
    printf("bool = %d\n", mmkv_decode_bool(kv, "bool", false));

    /* int32 */
    mmkv_encode_int32(kv, "int32", 1024);
    printf("int32 = %d\n", mmkv_decode_int32(kv, "int32", 0));

    /* uint32 */
    mmkv_encode_uint32(kv, "uint32", UINT32_MAX);
    printf("uint32 = %u\n", mmkv_decode_uint32(kv, "uint32", 0));

    /* int64 */
    mmkv_encode_int64(kv, "int64", INT64_MIN);
    printf("int64 = %" PRId64 "\n", mmkv_decode_int64(kv, "int64", 0));

    /* uint64 */
    mmkv_encode_uint64(kv, "uint64", UINT64_MAX);
    printf("uint64 = %" PRIu64 "\n", mmkv_decode_uint64(kv, "uint64", 0));

    /* float */
    mmkv_encode_float(kv, "float", 3.14f);
    printf("float = %f\n", (double) mmkv_decode_float(kv, "float", 0.0f));

    /* double */
    mmkv_encode_double(kv, "double", DBL_MAX);
    printf("double = %e\n", mmkv_decode_double(kv, "double", 0.0));

    /* string */
    mmkv_encode_string(kv, "string", "Hello, MMKV C Bridge!");
    {
        char *str = mmkv_decode_string(kv, "string");
        if (str) {
            printf("string = %s\n", str);
            mmkv_free(str);
        } else {
            printf("string = (null)\n");
        }
    }

    /* bytes */
    {
        const char data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        mmkv_encode_bytes(kv, "bytes", data, sizeof(data));
        uint64_t len = 0;
        void *buf = mmkv_decode_bytes(kv, "bytes", &len);
        if (buf) {
            printf("bytes length = %" PRIu64 ", first byte = 0x%02x\n",
                   len, ((unsigned char *)buf)[0]);
            mmkv_free(buf);
        }
    }

    /* containsKey */
    printf("containsKey[string] = %d\n", mmkv_contains_key(kv, "string"));
    printf("count = %" PRIu64 ", totalSize = %" PRIu64 "\n",
           mmkv_count(kv, false), mmkv_total_size(kv));

    /* allKeys */
    {
        uint64_t keyCount = 0;
        char **keys = mmkv_all_keys(kv, &keyCount, false);
        if (keys) {
            printf("allKeys (%"PRIu64"): ", keyCount);
            for (uint64_t i = 0; i < keyCount; i++) {
                printf("%s%s", keys[i], (i + 1 < keyCount) ? ", " : "");
                mmkv_free(keys[i]);
            }
            printf("\n");
            mmkv_free(keys);
        }
    }

    /* removeValue */
    mmkv_remove_value(kv, "bool");
    printf("bool after remove = %d\n", mmkv_decode_bool(kv, "bool", false));

    /* removeValues */
    {
        const char *keysToRemove[] = {"int32", "int64"};
        mmkv_remove_values(kv, keysToRemove, 2);
        printf("int32 after remove = %d\n", mmkv_decode_int32(kv, "int32", 0));
    }

    /* null string test */
    mmkv_encode_string(kv, "null_string", "some value");
    {
        char *s = mmkv_decode_string(kv, "null_string");
        printf("string before set null: %s\n", s ? s : "(null)");
        mmkv_free(s);
    }
    mmkv_encode_string(kv, "null_string", NULL);
    {
        char *s = mmkv_decode_string(kv, "null_string");
        printf("string after set null: %s, containsKey: %d\n",
               s ? s : "(null)", mmkv_contains_key(kv, "null_string"));
        mmkv_free(s);
    }

    /* clearMemoryCache + reload */
    mmkv_clear_memory_cache(kv, false);
    printf("count after clearMemoryCache = %" PRIu64 "\n", mmkv_count(kv, false));

    /* trim */
    mmkv_trim(kv);

    printf("=== Functional Test Done ===\n");
}

/* ── Encryption test ───────────────────────────────────────────────── */

static void encryptionTest(void) {
    printf("\n=== Encryption Test (C Bridge) ===\n");

    MMKVConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.cryptKey = "testCryptKey";
    cfg.enableKeyExpire = -1;
    cfg.recover = -1;

    MMKVHandle_t kv = mmkv_with_id("test_c_encrypt", cfg);
    if (!kv) {
        printf("Failed to create encrypted MMKV\n");
        return;
    }

    mmkv_encode_string(kv, "enc_key", "encrypted value");
    {
        char *s = mmkv_decode_string(kv, "enc_key");
        printf("encrypted string = %s\n", s ? s : "(null)");
        mmkv_free(s);
    }

    /* rekey to a different key */
    mmkv_rekey(kv, "newCryptKey", false);
    {
        uint32_t keyLen = 0;
        void *ck = mmkv_crypt_key(kv, &keyLen);
        if (ck) {
            printf("new cryptKey length = %u\n", keyLen);
            mmkv_free(ck);
        }
    }

    /* rekey to no encryption */
    mmkv_rekey(kv, NULL, false);
    {
        char *s = mmkv_decode_string(kv, "enc_key");
        printf("string after removing encryption = %s\n", s ? s : "(null)");
        mmkv_free(s);
    }

    printf("=== Encryption Test Done ===\n");
}

/* ── Value inspection & lock test ──────────────────────────────────── */

static void valueInspectionTest(MMKVHandle_t kv) {
    printf("\n=== Value Inspection & Lock Test (C Bridge) ===\n");

    mmkv_encode_string(kv, "inspect_str", "hello world");
    uint64_t size = mmkv_get_value_size(kv, "inspect_str", true);
    printf("getValueSize(inspect_str, actualSize=true) = %" PRIu64 "\n", size);
    size = mmkv_get_value_size(kv, "inspect_str", false);
    printf("getValueSize(inspect_str, actualSize=false) = %" PRIu64 "\n", size);

    /* writeValueToBuffer */
    {
        char buf[64];
        memset(buf, 0, sizeof(buf));
        int32_t written = mmkv_write_value_to_buffer(kv, "inspect_str", buf, sizeof(buf));
        printf("writeValueToBuffer returned %d, buf = \"%.*s\"\n", written, written > 0 ? written : 0, buf);
    }

    /* lock / unlock / try_lock */
    mmkv_lock(kv);
    printf("locked\n");
    mmkv_unlock(kv);
    printf("unlocked\n");
    bool got = mmkv_try_lock(kv);
    printf("try_lock = %d\n", got);
    if (got) { mmkv_unlock(kv); }

    /* feature queries */
    printf("isExpirationEnabled = %d\n", mmkv_is_expiration_enabled(kv));
    printf("isEncryptionEnabled = %d\n", mmkv_is_encryption_enabled(kv));
    printf("isCompareBeforeSetEnabled = %d\n", mmkv_is_compare_before_set_enabled(kv));

    mmkv_remove_value(kv, "inspect_str");
    printf("=== Value Inspection & Lock Test Done ===\n");
}

/* ── NameSpace test ────────────────────────────────────────────────── */

static void namespaceTest(void) {
    printf("\n=== NameSpace Test (C Bridge) ===\n");

    /* Create a namespace with custom root dir */
    MMKVNameSpace_t ns = mmkv_namespace("/tmp/mmkv_c_ns");
    if (!ns) {
        printf("Failed to create namespace\n");
        return;
    }
    printf("namespace rootDir: %s\n", mmkv_namespace_root_dir(ns));

    /* Create an instance within the namespace */
    MMKVConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enableKeyExpire = -1;
    cfg.recover = -1;
    MMKVHandle_t kv = mmkv_namespace_mmkv_with_id(ns, "ns_test", cfg);
    if (!kv) {
        printf("Failed to create MMKV in namespace\n");
        return;
    }

    mmkv_encode_string(kv, "ns_key", "namespace value");
    {
        char *s = mmkv_decode_string(kv, "ns_key");
        printf("ns_key = %s\n", s ? s : "(null)");
        mmkv_free(s);
    }
    printf("count = %" PRIu64 "\n", mmkv_count(kv, false));

    /* checkExist / isFileValid within namespace */
    printf("namespace checkExist(ns_test) = %d\n", mmkv_namespace_check_exist(ns, "ns_test"));
    printf("namespace isFileValid(ns_test) = %d\n", mmkv_namespace_is_file_valid(ns, "ns_test"));

    /* backup / restore within namespace */
    bool backupRet = mmkv_namespace_backup_one(ns, "ns_test", "/tmp/mmkv_c_ns_backup");
    printf("namespace backup one = %d\n", backupRet);

    mmkv_clear_all(kv, false);
    printf("count after clearAll = %" PRIu64 "\n", mmkv_count(kv, false));

    bool restoreRet = mmkv_namespace_restore_one(ns, "ns_test", "/tmp/mmkv_c_ns_backup");
    printf("namespace restore one = %d\n", restoreRet);
    printf("count after restore = %" PRIu64 "\n", mmkv_count(kv, false));
    {
        char *s = mmkv_decode_string(kv, "ns_key");
        printf("ns_key after restore = %s\n", s ? s : "(null)");
        mmkv_free(s);
    }

    /* removeStorage within namespace */
    mmkv_close(kv);
    bool removeRet = mmkv_namespace_remove_storage(ns, "ns_test");
    printf("namespace removeStorage = %d\n", removeRet);
    printf("namespace checkExist after remove = %d\n", mmkv_namespace_check_exist(ns, "ns_test"));

    /* default namespace */
    MMKVNameSpace_t defaultNs = mmkv_default_namespace();
    printf("default namespace rootDir: %s\n", mmkv_namespace_root_dir(defaultNs));
    mmkv_namespace_free(defaultNs);

    mmkv_namespace_free(ns);
    printf("=== NameSpace Test Done ===\n");
}

/* ── main ──────────────────────────────────────────────────────────── */

int main(void) {
    const char *rootDir = "/tmp/mmkv_c";

    MMKVHandler_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.log = myLogHandler;
    handler.error = myErrorHandler;

    mmkv_initialize_with_handler(rootDir, 1 /* MMKVLogInfo */, handler);

    printf("MMKV version: %s\n", mmkv_version());
    printf("MMKV rootDir: %s\n", mmkv_root_dir());
    printf("MMKV pageSize: %d\n", mmkv_page_size());

    /* Create a default instance */
    MMKVConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enableKeyExpire = -1;
    cfg.recover = -1;

    MMKVHandle_t kv = mmkv_with_id("test_c_bridge", cfg);
    if (!kv) {
        printf("Failed to create MMKV instance\n");
        return 1;
    }

    printf("mmapID: %s\n", mmkv_mmap_id(kv));
    printf("isMultiProcess: %d\n", mmkv_is_multi_process(kv));
    printf("isReadOnly: %d\n", mmkv_is_read_only(kv));

    functionalTest(kv);
    encryptionTest();
    valueInspectionTest(kv);
    namespaceTest();

    mmkv_on_exit();
    printf("\nAll C bridge tests passed.\n");
    return 0;
}
