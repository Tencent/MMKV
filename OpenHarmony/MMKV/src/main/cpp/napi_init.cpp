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
#include <system_error>

using namespace std;
using namespace mmkv;

// assuming env is defined
#define NAPI_CALL(call)                                                                                                \
    do {                                                                                                               \
        napi_status status = (call);                                                                                   \
        if (status != napi_ok) {                                                                                       \
            const napi_extended_error_info *error_info = nullptr;                                                      \
            napi_get_last_error_info(env, &error_info);                                                                \
            printf("NAPI Error: code %d, msg %s\n", error_info->error_code, error_info->error_message);                \
            bool is_pending;                                                                                           \
            napi_is_exception_pending(env, &is_pending);                                                               \
            if (!is_pending) {                                                                                         \
                auto message = error_info->error_message ? error_info->error_message : "null";                         \
                napi_throw_error(env, nullptr, message);                                                               \
                return nullptr;                                                                                        \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

static napi_value Add(napi_env env, napi_callback_info info) {
    size_t requireArgc = 2;
    size_t argc = 2;
    napi_value args[2] = {nullptr};

    NAPI_CALL(napi_get_cb_info(env, info, &argc, args , nullptr, nullptr));

    napi_valuetype valuetype0;
    NAPI_CALL(napi_typeof(env, args[0], &valuetype0));

    napi_valuetype valuetype1;
    NAPI_CALL(napi_typeof(env, args[1], &valuetype1));

    double value0;
    NAPI_CALL(napi_get_value_double(env, args[0], &value0));

    double value1;
    NAPI_CALL(napi_get_value_double(env, args[1], &value1));

    napi_value sum;
    NAPI_CALL(napi_create_double(env, value0 + value1, &sum));

    return sum;
}

static napi_value Version(napi_env env, napi_callback_info info) {
    napi_value sum;
    NAPI_CALL(napi_create_string_latin1(env, MMKV_VERSION, strlen(MMKV_VERSION), &sum));

    return sum;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "add", nullptr, Add, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "version", nullptr, Version, nullptr, nullptr, nullptr, napi_default, nullptr },
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

extern "C" __attribute__((constructor)) void RegisterMMKVModule(void)
{
    napi_module_register(&mmkvModule);
}
