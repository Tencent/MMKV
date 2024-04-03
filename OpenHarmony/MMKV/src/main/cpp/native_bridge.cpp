/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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

#include "MMBuffer.h"
#include "MMKV.h"
#include "MMKVLog.h"
#include "MemoryFile.h"
#include "napi/native_api.h"
#include <cstdint>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <hilog/log.h>

#define LOG_LEVEL(level, ...) OH_LOG_Print(LOG_APP, level, 0, "mmkv", __VA_ARGS__)
#define MMKV_LOG_DEBUG(...) LOG_LEVEL(LOG_DEBUG, __VA_ARGS__)
#define MMKV_LOG_INFO(...) LOG_LEVEL(LOG_INFO, __VA_ARGS__)
#define MMKV_LOG_WARN(...) LOG_LEVEL(LOG_WARN, __VA_ARGS__)
#define MMKV_LOG_ERROR(...) LOG_LEVEL(LOG_ERROR, __VA_ARGS__)

using namespace std;
using namespace mmkv;

// assuming env is defined
#define NAPI_CALL_RET(call, return_value)                                                                              \
    do {                                                                                                               \
        napi_status status = (call);                                                                                   \
        if (status != napi_ok) {                                                                                       \
            const napi_extended_error_info *error_info = nullptr;                                                      \
            napi_get_last_error_info(env, &error_info);                                                                \
            MMKV_LOG_INFO("NAPI Error: code %{public}d, msg %{public}s\n",                                             \
                error_info->error_code, error_info->error_message);                                                    \
            bool is_pending;                                                                                           \
            napi_is_exception_pending(env, &is_pending);                                                               \
            if (!is_pending) {                                                                                         \
                auto message = error_info->error_message ? error_info->error_message : "null";                         \
                napi_throw_error(env, nullptr, message);                                                               \
                return return_value;                                                                                        \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

#define NAPI_CALL(call) NAPI_CALL_RET(call, nullptr)

bool IsNValueUndefined(napi_env env, napi_value value) {
    napi_valuetype type;
    if (napi_typeof(env, value, &type) == napi_ok && type == napi_undefined) {
        return true;
    }
    return false;
}

static string NAPIString2String(napi_env env, napi_value value, bool maybeUndefined = false) {
    if (maybeUndefined && IsNValueUndefined(env, value)) {
        return "";
    }

    string result;
    size_t size;
    NAPI_CALL_RET(napi_get_value_string_utf8(env, value, nullptr, 0, &size), "");
    result.resize(size);
    // MMKV_LOG_INFO("size: %{public}zu, capacity: %{public}zu\n", result.size(), result.capacity());
    NAPI_CALL_RET(napi_get_value_string_utf8(env, value, result.data(), result.capacity(), nullptr), "");
    return result;
}

static napi_value String2NAPIString(napi_env env, const string &value) {
    napi_value result;
    napi_create_string_utf8(env, value.data(), value.size(), &result);
    return result;
}

static napi_value NAPIUndefined(napi_env env) {
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

static napi_value NAPINull(napi_env env) {
    napi_value result;
    napi_get_null(env, &result);
    return result;
}

static napi_value NAPIBool(napi_env env, double resultDoubleValue) {
    napi_value result;
    napi_value resultBool;
    napi_create_double(env, resultDoubleValue, &result);
    napi_coerce_to_bool(env, result, &resultBool);
    return resultBool;
}

static bool NValueToBool(napi_env env, napi_value value) {
    bool result;
    napi_get_value_bool(env, value, &result);
    return result;
}

static uint32_t NValueToUInt32(napi_env env, napi_value value) {
    uint32_t result;
    napi_get_value_uint32(env, value, &result);
    return result;
}

static napi_value UInt64ToNValue(napi_env env, uint64_t value) {
    napi_value result;
    napi_create_bigint_uint64(env, value, &result);
    return result;
}

static uint64_t NValueToUInt64(napi_env env, napi_value value) {
    uint64_t result;
    bool lossless;
    napi_get_value_bigint_uint64(env, value, &result, &lossless);
    return result;
}

static napi_value initialize(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    NAPI_CALL(napi_get_cb_info(env, info, &argc, args , nullptr, nullptr));

    auto rootDir = NAPIString2String(env, args[0]);
    auto cacheDir = NAPIString2String(env, args[1]);

    int32_t logLevel;
    NAPI_CALL(napi_get_value_int32(env, args[2], &logLevel));

    MMKV_LOG_INFO("rootDir: %{public}s, cacheDir: %{public}s, log level:%{public}d\n",
        rootDir.c_str(), cacheDir.c_str(), logLevel);

    MMKV::initializeMMKV(rootDir, (MMKVLogLevel) logLevel);
    g_android_tmpDir = cacheDir;

    return String2NAPIString(env, MMKV::getRootDir());
}

static napi_value version(napi_env env, napi_callback_info info) {
    napi_value sum;
    NAPI_CALL(napi_create_string_latin1(env, MMKV_VERSION, strlen(MMKV_VERSION), &sum));

    return sum;
}

static napi_value getDefaultMMKV(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    NAPI_CALL(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    int32_t mode;
    NAPI_CALL(napi_get_value_int32(env, args[0], &mode));
    auto crypt = NAPIString2String(env, args[1], true);

    MMKV *kv = nullptr;
    if (crypt.length() > 0) {
        kv = MMKV::defaultMMKV((MMKVMode)mode, &crypt);
    }

    if (!kv) {
        kv = MMKV::defaultMMKV((MMKVMode)mode, nullptr);
    }

    return UInt64ToNValue(env,(uint64_t)kv);
}

static napi_value encodeBool(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4] = {nullptr};
    NAPI_CALL(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    auto handle = NValueToUInt64(env, args[0]);
    auto key = NAPIString2String(env, args[1]);
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && key.length() > 0) {
        auto value = NValueToBool(env,args[2]);
        if (IsNValueUndefined(env, args[3])) {
            auto ret = kv->set(value, key);
            return NAPIBool(env, ret);
        }
        uint32_t expiration = NValueToUInt32(env, args[3]);
        auto ret = kv->set(value, key, expiration);
        return NAPIBool(env, ret);
    }
    return NAPIBool(env,false);
}

static napi_value decodeBool(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    NAPI_CALL(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    auto handle = NValueToUInt64(env, args[0]);
    auto key = NAPIString2String(env, args[1]);
    auto defaultValue = NValueToBool(env, args[2]);
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && key.length() > 0) {
        return NAPIBool(env, kv->getBool(key, defaultValue));
    }
    return NAPIBool(env, defaultValue);
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "initialize", nullptr, initialize, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "version", nullptr, version, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getDefaultMMKV", nullptr, getDefaultMMKV, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "encodeBool", nullptr, encodeBool, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "decodeBool", nullptr, decodeBool, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module mmkvModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "mmkv",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterMMKVModule(void) {
    napi_module_register(&mmkvModule);
}
