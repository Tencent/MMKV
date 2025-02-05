/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2025 THL A29 Limited, a Tencent company.
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

// demo of using MMKV C++ ABI directly

#include "napi/native_api.h"
#include <MMKV/MMKV.h>
#include <hilog/log.h>
#include <string>

using namespace std;

bool IsNValueUndefined(napi_env env, napi_value value) {
    napi_valuetype type;
    if (napi_typeof(env, value, &type) == napi_ok && type == napi_undefined) {
        return true;
    }
    return false;
}

static string NValueToString(napi_env env, napi_value value, bool maybeUndefined = false) {
    if (maybeUndefined && IsNValueUndefined(env, value)) {
        return "";
    }

    size_t size;
    napi_get_value_string_utf8(env, value, nullptr, 0, &size);
    string result(size, '\0');
    napi_get_value_string_utf8(env, value, (char *) result.data(), size + 1, nullptr);
    return result;
}

static napi_value StringToNValue(napi_env env, const string &value) {
    napi_value result;
    napi_create_string_utf8(env, value.data(), value.size(), &result);
    return result;
}

static napi_value BoolToNValue(napi_env env, bool value) {
    napi_value result;
    napi_value resultBool;
    napi_create_double(env, value, &result);
    napi_coerce_to_bool(env, result, &resultBool);
    return resultBool;
}

static napi_value UInt64ToNValue(napi_env env, uint64_t value) {
    napi_value result;
    napi_create_bigint_uint64(env, value, &result);
    return result;
}

void _MMKVLogWithLevel(const char *filename, const char *func, int line, const char *format, ...) {
    string message;
    char buffer[16];

    va_list args;
    va_start(args, format);
    auto length = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0) { // something wrong
        message = {};
    } else if (length < sizeof(buffer)) {
        message = string(buffer, static_cast<unsigned long>(length));
    } else {
        message.resize(static_cast<unsigned long>(length), '\0');
        va_start(args, format);
        std::vsnprintf(const_cast<char *>(message.data()), static_cast<size_t>(length) + 1, format, args);
        va_end(args);
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, 0, "mmkvdemo", "<%{public}s:%{public}d::%{public}s> %{public}s",
        filename, line, func, message.c_str());
}

#define MMKVLog(format, ...) \
        _MMKVLogWithLevel(__FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)

string to_string(const std::string& str) {  return str; }

template <class T>
string to_string(const vector<T> &arr, const char* sp = ", ") {
    string str;
    for (const auto &element : arr) {
        str += to_string(element);
        str += sp;
    }
    if (!str.empty()) {
        str.erase(str.length() - strlen(sp));
    }
    return str;
}

void functionalTest(MMKV *mmkv, bool decodeOnly) {
    if (!decodeOnly) {
        mmkv->set(true, "bool");
    }
    MMKVLog("bool = %d\n", mmkv->getBool("bool"));

    if (!decodeOnly) {
        mmkv->set(1024, "int32");
    }
    MMKVLog("int32 = %d\n", mmkv->getInt32("int32"));

    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<uint32_t>::max(), "uint32");
    }
    MMKVLog("uint32 = %u\n", mmkv->getUInt32("uint32"));

    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<int64_t>::min(), "int64");
    }
    MMKVLog("int64 = %lld\n", mmkv->getInt64("int64"));

    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<uint64_t>::max(), "uint64");
    }
    MMKVLog("uint64 = %llu\n", mmkv->getUInt64("uint64"));

    if (!decodeOnly) {
        mmkv->set(3.14f, "float");
    }
    MMKVLog("float = %f\n", mmkv->getFloat("float"));

    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<double>::max(), "double");
    }
    MMKVLog("double = %f\n", mmkv->getDouble("double"));

    if (!decodeOnly) {
        mmkv->set("Hello, MMKV-示例 for POSIX", "raw_string");
        std::string str = "Hello, MMKV-示例 for POSIX string";
        mmkv->set(str, "string");
        mmkv->set(std::string_view(str).substr(7, 21), "string_view");
    }
    std::string result;
    mmkv->getString("raw_string", result);
    MMKVLog("raw_string = %s\n", result.c_str());
    mmkv->getString("string", result);
    MMKVLog("string = %s\n", result.c_str());
    mmkv->getString("string_view", result);
    MMKVLog("string_view = %s\n", result.c_str());

    // containerTest(mmkv, decodeOnly);

    MMKVLog("allKeys: %s\n", ::to_string(mmkv->allKeys()).c_str());
    MMKVLog("count = %zu, totalSize = %zu\n", mmkv->count(), mmkv->totalSize());
    MMKVLog("containsKey[string]: %d\n", mmkv->containsKey("string"));

    mmkv->removeValueForKey("bool");
    MMKVLog("bool: %d\n", mmkv->getBool("bool"));
    mmkv->removeValuesForKeys({"int", "long"});

    mmkv->set("some string", "null string");
    result.erase();
    mmkv->getString("null string", result);
    MMKVLog("string before set null: %s\n", result.c_str());
    mmkv->set((const char *) nullptr, "null string");
    //mmkv->set("", "null string");
    result.erase();
    mmkv->getString("null string", result);
    MMKVLog("string after set null: %s, containsKey: %d\n", result.c_str(), mmkv->containsKey("null string"));

    //kv.sync();
    //kv.async();
    //kv.clearAll();
    mmkv->clearMemoryCache();
    MMKVLog("allKeys: %s\n", ::to_string(mmkv->allKeys()).c_str());
    MMKVLog("isFileValid[%s]: %d\n", mmkv->mmapID().c_str(), MMKV::isFileValid(mmkv->mmapID()));
}

static napi_value TestNativeMMKV(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = {nullptr};

    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);

    string root = NValueToString(env, args[0]);
    string mmapID = NValueToString(env, args[1]);
    auto ns = MMKV::nameSpace(root);
    auto kv = ns.mmkvWithID(mmapID);
    functionalTest(kv, false);

    auto result = mmkv::DEFAULT_MMAP_SIZE;

    return UInt64ToNValue(env, result);
}


EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "TestNativeMMKV", nullptr, TestNativeMMKV, nullptr, nullptr, nullptr, napi_default, nullptr }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
