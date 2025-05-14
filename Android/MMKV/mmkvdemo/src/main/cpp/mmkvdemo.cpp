/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
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

// A demo for using MMKV C++ interface in native code

#include <string>
#include <jni.h>
#include <android/log.h>
#ifdef ENABLE_MMKV_NATIVE
#include <MMKV/MMKV.h>
#endif
using namespace std;

constexpr auto APP_NAME = "MMKVNativeDemo";

void _MMKVLogWithLevel(android_LogPriority level, const char *filename, const char *func, int line, const char *format, ...) {
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

    __android_log_print(level, APP_NAME, "<%s:%d::%s> %s", filename, line, func, message.c_str());
}

#define MMKVError(format, ...) \
    _MMKVLogWithLevel(ANDROID_LOG_ERROR, __FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)
#define MMKVWarning(format, ...) \
    _MMKVLogWithLevel(ANDROID_LOG_WARN, __FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)
#define MMKVInfo(format, ...) \
    _MMKVLogWithLevel(ANDROID_LOG_INFO, __FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)
#define MMKVLog MMKVInfo

static string jstring2string(JNIEnv *env, jstring str) {
    if (str) {
        const char *kstr = env->GetStringUTFChars(str, nullptr);
        if (kstr) {
            string result(kstr);
            env->ReleaseStringUTFChars(str, kstr);
            return result;
        }
    }
    return "";
}

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

#ifdef ENABLE_MMKV_NATIVE

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
#endif
static void testNameSpaceInNative(JNIEnv *env, jobject obj, jstring rootDir, jstring mmapID) {
    string root = jstring2string(env, rootDir);
    string id = jstring2string(env, mmapID);
#ifdef ENABLE_MMKV_NATIVE
    auto ns = MMKV::nameSpace(root);
    auto kv = ns.mmkvWithID(id);
    functionalTest(kv, false);
#else
    MMKVWarning("ENABLE_MMKV_NATIVE not defined.");
#endif
}

#ifndef ENABLE_MMKV_NATIVE
enum MMKVLogLevel : int {
    MMKVLogDebug = 0, // not available for release/product build
    MMKVLogInfo = 1,  // default level
    MMKVLogWarning,
    MMKVLogError,
    MMKVLogNone, // special level used to disable all log messages
};
#endif

static android_LogPriority MMKVLogLevelDesc(MMKVLogLevel level) {
    switch (level) {
    case MMKVLogDebug:
        return ANDROID_LOG_DEBUG;
    case MMKVLogInfo:
        return ANDROID_LOG_INFO;
    case MMKVLogWarning:
        return ANDROID_LOG_WARN;
    case MMKVLogError:
        return ANDROID_LOG_ERROR;
    default:
        return ANDROID_LOG_UNKNOWN;
    }
}

static void mmkvLog(MMKVLogLevel level, const char *file, int line, const char *function, const char *message) {
    auto desc = MMKVLogLevelDesc(level);
    __android_log_print(desc, APP_NAME, "<%s:%d::%s> %s", file, line, function, message);
}

static jlong getNativeLogHandler(JNIEnv *env, jobject obj) {
    return (jlong) mmkvLog;
}

static JNINativeMethod g_methods[] = {
    {"testNameSpaceInNative", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) testNameSpaceInNative},
    {"getNativeLogHandler", "()J", (void *) getNativeLogHandler},
};

static int registerNativeMethods(JNIEnv *env, jclass cls) {
    return env->RegisterNatives(cls, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
}

extern "C" JNIEXPORT JNICALL jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    static const char *clsName = "com/tencent/mmkvdemo/MyApplication";
    jclass instance = env->FindClass(clsName);
    if (!instance) {
        MMKVError("fail to locate class: %s", clsName);
        return -2;
    }

    int ret = registerNativeMethods(env, instance);
    if (ret != 0) {
        MMKVError("fail to register native methods for class %s, ret = %d", clsName, ret);
        return -4;
    }
    return JNI_VERSION_1_6;
}
