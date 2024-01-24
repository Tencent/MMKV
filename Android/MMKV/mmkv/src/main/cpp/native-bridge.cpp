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

#include "MMKVPredef.h"

#ifdef MMKV_ANDROID

#    include "MMBuffer.h"
#    include "MMKV.h"
#    include "MMKVLog.h"
#    include "MemoryFile.h"
#    include <cstdint>
#    include <jni.h>
#    include <string>

using namespace std;
using namespace mmkv;

static jclass g_cls = nullptr;
static jfieldID g_fileID = nullptr;
static jmethodID g_callbackOnCRCFailID = nullptr;
static jmethodID g_callbackOnFileLengthErrorID = nullptr;
static jmethodID g_mmkvLogID = nullptr;
static jmethodID g_callbackOnContentChange = nullptr;
static JavaVM *g_currentJVM = nullptr;

static int registerNativeMethods(JNIEnv *env, jclass cls);
extern "C" void internalLogWithLevel(MMKVLogLevel level, const char *filename, const char *func, int line, const char *format, ...);
extern MMKVLogLevel g_currentLogLevel;

namespace mmkv {
    static void mmkvLog(MMKVLogLevel level, const char *file, int line, const char *function, const std::string &message);
}

#define InternalLogError(format, ...) \
    internalLogWithLevel(MMKV_NAMESPACE_PREFIX::MMKVLogError, __MMKV_FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)

#define InternalLogInfo(format, ...) \
    internalLogWithLevel(MMKV_NAMESPACE_PREFIX::MMKVLogInfo, __MMKV_FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)

extern "C" JNIEXPORT JNICALL jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    g_currentJVM = vm;
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    if (g_cls) {
        env->DeleteGlobalRef(g_cls);
    }
    static const char *clsName = "com/tencent/mmkv/MMKV";
    jclass instance = env->FindClass(clsName);
    if (!instance) {
        MMKVError("fail to locate class: %s", clsName);
        return -2;
    }
    g_cls = reinterpret_cast<jclass>(env->NewGlobalRef(instance));
    if (!g_cls) {
        MMKVError("fail to create global reference for %s", clsName);
        return -3;
    }
    g_mmkvLogID =
        env->GetStaticMethodID(g_cls, "mmkvLogImp", "(ILjava/lang/String;ILjava/lang/String;Ljava/lang/String;)V");
    if (!g_mmkvLogID) {
        MMKVError("fail to get method id for mmkvLogImp");
    }
    // every code from now on can use InternalLogXXX()

    int ret = registerNativeMethods(env, g_cls);
    if (ret != 0) {
        InternalLogError("fail to register native methods for class %s, ret = %d", clsName, ret);
        return -4;
    }
    g_fileID = env->GetFieldID(g_cls, "nativeHandle", "J");
    if (!g_fileID) {
        InternalLogError("fail to locate fileID");
        return -5;
    }

    g_callbackOnCRCFailID = env->GetStaticMethodID(g_cls, "onMMKVCRCCheckFail", "(Ljava/lang/String;)I");
    if (!g_callbackOnCRCFailID) {
        InternalLogError("fail to get method id for onMMKVCRCCheckFail");
    }
    g_callbackOnFileLengthErrorID = env->GetStaticMethodID(g_cls, "onMMKVFileLengthError", "(Ljava/lang/String;)I");
    if (!g_callbackOnFileLengthErrorID) {
        InternalLogError("fail to get method id for onMMKVFileLengthError");
    }
    g_callbackOnContentChange =
        env->GetStaticMethodID(g_cls, "onContentChangedByOuterProcess", "(Ljava/lang/String;)V");
    if (!g_callbackOnContentChange) {
        InternalLogError("fail to get method id for onContentChangedByOuterProcess()");
    }

    // get current API level by accessing android.os.Build.VERSION.SDK_INT
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (versionClass) {
        jfieldID sdkIntFieldID = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
        if (sdkIntFieldID) {
            g_android_api = env->GetStaticIntField(versionClass, sdkIntFieldID);
#ifdef MMKV_STL_SHARED
            InternalLogInfo("current API level = %d, libc++_shared=%d", g_android_api, MMKV_STL_SHARED);
#else
            InternalLogInfo("current API level = %d, libc++_shared=?", g_android_api);
#endif
        } else {
            InternalLogError("fail to get field id android.os.Build.VERSION.SDK_INT");
        }
    } else {
        InternalLogError("fail to get class android.os.Build.VERSION");
    }

    return JNI_VERSION_1_6;
}

//#define MMKV_JNI extern "C" JNIEXPORT JNICALL
#    define MMKV_JNI static

namespace mmkv {

static string jstring2string(JNIEnv *env, jstring str);

MMKV_JNI void jniInitialize(JNIEnv *env, jobject obj, jstring rootDir, jstring cacheDir, jint logLevel) {
    if (!rootDir) {
        return;
    }
    const char *kstr = env->GetStringUTFChars(rootDir, nullptr);
    if (kstr) {
        MMKV::initializeMMKV(kstr, (MMKVLogLevel) logLevel);
        env->ReleaseStringUTFChars(rootDir, kstr);

        g_android_tmpDir = jstring2string(env, cacheDir);
    }
}

MMKV_JNI void jniInitialize_2(JNIEnv *env, jobject obj, jstring rootDir, jstring cacheDir, jint logLevel, jboolean logReDirecting) {
    if (!rootDir) {
        return;
    }
    const char *kstr = env->GetStringUTFChars(rootDir, nullptr);
    if (kstr) {
        auto logHandler = logReDirecting ? mmkvLog : nullptr;
        MMKV::initializeMMKV(kstr, (MMKVLogLevel) logLevel, logHandler);
        env->ReleaseStringUTFChars(rootDir, kstr);

        g_android_tmpDir = jstring2string(env, cacheDir);
    }
}

MMKV_JNI void onExit(JNIEnv *env, jobject obj) {
    MMKV::onExit();
}

static MMKV *getMMKV(JNIEnv *env, jobject obj) {
    jlong handle = env->GetLongField(obj, g_fileID);
    return reinterpret_cast<MMKV *>(handle);
}

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

static jstring string2jstring(JNIEnv *env, const string &str) {
    return env->NewStringUTF(str.c_str());
}

static vector<string> jarray2vector(JNIEnv *env, jobjectArray array) {
    vector<string> keys;
    if (array) {
        jsize size = env->GetArrayLength(array);
        keys.reserve(size);
        for (jsize i = 0; i < size; i++) {
            jstring str = (jstring) env->GetObjectArrayElement(array, i);
            if (str) {
                keys.push_back(jstring2string(env, str));
                env->DeleteLocalRef(str);
            }
        }
    }
    return keys;
}

static jobjectArray vector2jarray(JNIEnv *env, const vector<string> &arr) {
    jobjectArray result = env->NewObjectArray(arr.size(), env->FindClass("java/lang/String"), nullptr);
    if (result) {
        for (size_t index = 0; index < arr.size(); index++) {
            jstring value = string2jstring(env, arr[index]);
            env->SetObjectArrayElement(result, index, value);
            env->DeleteLocalRef(value);
        }
    }
    return result;
}

static JNIEnv *getCurrentEnv() {
    if (g_currentJVM) {
        JNIEnv *currentEnv = nullptr;
        auto ret = g_currentJVM->GetEnv(reinterpret_cast<void **>(&currentEnv), JNI_VERSION_1_6);
        if (ret == JNI_OK) {
            return currentEnv;
        } else {
            MMKVError("fail to get current JNIEnv: %d", ret);
        }
    }
    return nullptr;
}

MMKVRecoverStrategic onMMKVError(const std::string &mmapID, MMKVErrorType errorType) {
    jmethodID methodID = nullptr;
    if (errorType == MMKVCRCCheckFail) {
        methodID = g_callbackOnCRCFailID;
    } else if (errorType == MMKVFileLength) {
        methodID = g_callbackOnFileLengthErrorID;
    }

    auto currentEnv = getCurrentEnv();
    if (currentEnv && methodID) {
        jstring str = string2jstring(currentEnv, mmapID);
        auto strategic = currentEnv->CallStaticIntMethod(g_cls, methodID, str);
        return static_cast<MMKVRecoverStrategic>(strategic);
    }
    return OnErrorDiscard;
}

extern "C" void internalLogWithLevel(MMKVLogLevel level, const char *filename, const char *func, int line, const char *format, ...) {
    if (level >= g_currentLogLevel) {
        std::string message;
        char buffer[16];

        va_list args;
        va_start(args, format);
        auto length = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (length < 0) { // something wrong
            message = {};
        } else if (length < sizeof(buffer)) {
            message = std::string(buffer, static_cast<unsigned long>(length));
        } else {
            message.resize(static_cast<unsigned long>(length), '\0');
            va_start(args, format);
            std::vsnprintf(const_cast<char *>(message.data()), static_cast<size_t>(length) + 1, format, args);
            va_end(args);
        }

        if (g_cls && g_mmkvLogID) {
            mmkvLog(level, filename, line, func, message);
        } else {
            _MMKVLogWithLevel(level, filename, func, line, message.c_str());
        }
    }
}

static void mmkvLog(MMKVLogLevel level, const char *file, int line, const char *function, const std::string &message) {
    auto currentEnv = getCurrentEnv();
    if (currentEnv && g_mmkvLogID) {
        jstring oFile = string2jstring(currentEnv, string(file));
        jstring oFunction = string2jstring(currentEnv, string(function));
        jstring oMessage = string2jstring(currentEnv, message);
        int readLevel = level;
        currentEnv->CallStaticVoidMethod(g_cls, g_mmkvLogID, readLevel, oFile, line, oFunction, oMessage);
    }
}

static void onContentChangedByOuterProcess(const std::string &mmapID) {
    auto currentEnv = getCurrentEnv();
    if (currentEnv && g_callbackOnContentChange) {
        jstring str = string2jstring(currentEnv, mmapID);
        currentEnv->CallStaticVoidMethod(g_cls, g_callbackOnContentChange, str);
    }
}

MMKV_JNI jlong getMMKVWithID(JNIEnv *env, jobject, jstring mmapID, jint mode, jstring cryptKey, jstring rootPath,
                             jlong expectedCapacity) {
    MMKV *kv = nullptr;
    if (!mmapID) {
        return (jlong) kv;
    }
    string str = jstring2string(env, mmapID);

    bool done = false;
    if (cryptKey) {
        string crypt = jstring2string(env, cryptKey);
        if (crypt.length() > 0) {
            if (rootPath) {
                string path = jstring2string(env, rootPath);
                kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, &crypt, &path, expectedCapacity);
            } else {
                kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, &crypt, nullptr, expectedCapacity);
            }
            done = true;
        }
    }
    if (!done) {
        if (rootPath) {
            string path = jstring2string(env, rootPath);
            kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, nullptr, &path, expectedCapacity);
        } else {
            kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, nullptr, nullptr, expectedCapacity);
        }
    }

    return (jlong) kv;
}

MMKV_JNI jlong getMMKVWithIDAndSize(JNIEnv *env, jobject obj, jstring mmapID, jint size, jint mode, jstring cryptKey) {
    MMKV *kv = nullptr;
    if (!mmapID || size < 0) {
        return (jlong) kv;
    }
    string str = jstring2string(env, mmapID);

    if (cryptKey) {
        string crypt = jstring2string(env, cryptKey);
        if (crypt.length() > 0) {
            kv = MMKV::mmkvWithID(str, size, (MMKVMode) mode, &crypt);
        }
    }
    if (!kv) {
        kv = MMKV::mmkvWithID(str, size, (MMKVMode) mode, nullptr);
    }
    return (jlong) kv;
}

MMKV_JNI jlong getDefaultMMKV(JNIEnv *env, jobject obj, jint mode, jstring cryptKey) {
    MMKV *kv = nullptr;

    if (cryptKey) {
        string crypt = jstring2string(env, cryptKey);
        if (crypt.length() > 0) {
            kv = MMKV::defaultMMKV((MMKVMode) mode, &crypt);
        }
    }
    if (!kv) {
        kv = MMKV::defaultMMKV((MMKVMode) mode, nullptr);
    }

    return (jlong) kv;
}

MMKV_JNI jlong getMMKVWithAshmemFD(JNIEnv *env, jobject obj, jstring mmapID, jint fd, jint metaFD, jstring cryptKey) {
    MMKV *kv = nullptr;
    if (!mmapID || fd < 0 || metaFD < 0) {
        return (jlong) kv;
    }
    string id = jstring2string(env, mmapID);

    if (cryptKey) {
        string crypt = jstring2string(env, cryptKey);
        if (crypt.length() > 0) {
            kv = MMKV::mmkvWithAshmemFD(id, fd, metaFD, &crypt);
        }
    }
    if (!kv) {
        kv = MMKV::mmkvWithAshmemFD(id, fd, metaFD, nullptr);
    }

    return (jlong) kv;
}

MMKV_JNI jstring mmapID(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return string2jstring(env, kv->mmapID());
    }
    return nullptr;
}

MMKV_JNI jint ashmemFD(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return kv->ashmemFD();
    }
    return -1;
}

MMKV_JNI jint ashmemMetaFD(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return kv->ashmemMetaFD();
    }
    return -1;
}

MMKV_JNI jboolean checkProcessMode(JNIEnv *env, jobject, jlong handle) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        return kv->checkProcessMode();
    }
    return false;
}

MMKV_JNI jboolean encodeBool(JNIEnv *env, jobject, jlong handle, jstring oKey, jboolean value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((bool) value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeBool_2(JNIEnv *env, jobject, jlong handle, jstring oKey, jboolean value, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((bool) value, key, (uint32_t) expiration);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean decodeBool(JNIEnv *env, jobject, jlong handle, jstring oKey, jboolean defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->getBool(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeInt(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((int32_t) value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeInt_2(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint value, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((int32_t) value, key, (uint32_t) expiration);
    }
    return (jboolean) false;
}

MMKV_JNI jint decodeInt(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jint) kv->getInt32(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeLong(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((int64_t) value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeLong_2(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong value, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((int64_t) value, key, (uint32_t) expiration);
    }
    return (jboolean) false;
}

MMKV_JNI jlong decodeLong(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jlong) kv->getInt64(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeFloat(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jfloat value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((float) value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeFloat_2(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jfloat value, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((float) value, key, (uint32_t) expiration);
    }
    return (jboolean) false;
}

MMKV_JNI jfloat decodeFloat(JNIEnv *env, jobject, jlong handle, jstring oKey, jfloat defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jfloat) kv->getFloat(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeDouble(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jdouble value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((double) value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeDouble_2(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jdouble value, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->set((double) value, key, (uint32_t) expiration);
    }
    return (jboolean) false;
}

MMKV_JNI jdouble decodeDouble(JNIEnv *env, jobject, jlong handle, jstring oKey, jdouble defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jdouble) kv->getDouble(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeString(JNIEnv *env, jobject, jlong handle, jstring oKey, jstring oValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (oValue) {
            string value = jstring2string(env, oValue);
            return (jboolean) kv->set(value, key);
        } else {
            kv->removeValueForKey(key);
            return (jboolean) true;
        }
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeString_2(JNIEnv *env, jobject, jlong handle, jstring oKey, jstring oValue, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (oValue) {
            string value = jstring2string(env, oValue);
            return (jboolean) kv->set(value, key, (uint32_t) expiration);
        } else {
            kv->removeValueForKey(key);
            return (jboolean) true;
        }
    }
    return (jboolean) false;
}

MMKV_JNI jstring decodeString(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jstring oDefaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        string value;
        bool hasValue = kv->getString(key, value);
        if (hasValue) {
            return string2jstring(env, value);
        }
    }
    return oDefaultValue;
}

MMKV_JNI jboolean encodeBytes(JNIEnv *env, jobject, jlong handle, jstring oKey, jbyteArray oValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (oValue) {
            MMBuffer value(0);
            {
                jsize len = env->GetArrayLength(oValue);
                void *bufferPtr = env->GetPrimitiveArrayCritical(oValue, nullptr);
                if (bufferPtr) {
                    value = MMBuffer(bufferPtr, len);
                    env->ReleasePrimitiveArrayCritical(oValue, bufferPtr, JNI_ABORT);
                } else {
                    MMKVError("fail to get array: %s=%p", key.c_str(), oValue);
                }
            }
            return (jboolean) kv->set(value, key);
        } else {
            kv->removeValueForKey(key);
            return (jboolean) true;
        }
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeBytes_2(JNIEnv *env, jobject, jlong handle, jstring oKey, jbyteArray oValue, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (oValue) {
            MMBuffer value(0);
            {
                jsize len = env->GetArrayLength(oValue);
                void *bufferPtr = env->GetPrimitiveArrayCritical(oValue, nullptr);
                if (bufferPtr) {
                    value = MMBuffer(bufferPtr, len);
                    env->ReleasePrimitiveArrayCritical(oValue, bufferPtr, JNI_ABORT);
                } else {
                    MMKVError("fail to get array: %s=%p", key.c_str(), oValue);
                }
            }
            return (jboolean) kv->set(value, key, (uint32_t) expiration);
        } else {
            kv->removeValueForKey(key);
            return (jboolean) true;
        }
    }
    return (jboolean) false;
}

MMKV_JNI jbyteArray decodeBytes(JNIEnv *env, jobject obj, jlong handle, jstring oKey) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        mmkv::MMBuffer value;
        auto hasValue = kv->getBytes(key, value);
        if (hasValue) {
            jbyteArray result = env->NewByteArray(value.length());
            env->SetByteArrayRegion(result, 0, value.length(), (const jbyte *) value.getPtr());
            return result;
        }
    }
    return nullptr;
}

MMKV_JNI jobjectArray allKeys(JNIEnv *env, jobject instance, jlong handle, jboolean filterExpire) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        vector<string> keys = kv->allKeys((bool) filterExpire);
        return vector2jarray(env, keys);
    }
    return nullptr;
}

MMKV_JNI jboolean containsKey(JNIEnv *env, jobject instance, jlong handle, jstring oKey) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->containsKey(key);
    }
    return (jboolean) false;
}

MMKV_JNI jlong count(JNIEnv *env, jobject instance, jlong handle, jboolean filterExpire) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->count((bool) filterExpire);
        return size;
    }
    return 0;
}

MMKV_JNI jlong totalSize(JNIEnv *env, jobject instance, jlong handle) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->totalSize();
        return size;
    }
    return 0;
}

MMKV_JNI jlong actualSize(JNIEnv *env, jobject instance, jlong handle) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->actualSize();
        return size;
    }
    return 0;
}

MMKV_JNI void removeValueForKey(JNIEnv *env, jobject instance, jlong handle, jstring oKey) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        kv->removeValueForKey(key);
    }
}

MMKV_JNI void removeValuesForKeys(JNIEnv *env, jobject instance, jobjectArray arrKeys) {
    MMKV *kv = getMMKV(env, instance);
    if (kv && arrKeys) {
        vector<string> keys = jarray2vector(env, arrKeys);
        if (!keys.empty()) {
            kv->removeValuesForKeys(keys);
        }
    }
}

MMKV_JNI void clearAll(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->clearAll();
    }
}

MMKV_JNI void sync(JNIEnv *env, jobject instance, jboolean sync) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->sync((SyncFlag) sync);
    }
}

MMKV_JNI jboolean isFileValid(JNIEnv *env, jclass type, jstring oMmapID, jstring rootPath) {
    if (oMmapID) {
        string mmapID = jstring2string(env, oMmapID);
        if (!rootPath) {
            return (jboolean) MMKV::isFileValid(mmapID, nullptr);
        } else {
            auto root = jstring2string(env, rootPath);
            return (jboolean) MMKV::isFileValid(mmapID, &root);
        }
    }
    return (jboolean) false;
}

MMKV_JNI jboolean removeStorage(JNIEnv *env, jclass type, jstring oMmapID, jstring rootPath) {
    if (oMmapID) {
        string mmapID = jstring2string(env, oMmapID);
        if (!rootPath) {
            return (jboolean) MMKV::removeStorage(mmapID, nullptr);
        } else {
            auto root = jstring2string(env, rootPath);
            return (jboolean) MMKV::removeStorage(mmapID, &root);
        }
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeSet(JNIEnv *env, jobject, jlong handle, jstring oKey, jobjectArray arrStr) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (arrStr) {
            vector<string> value = jarray2vector(env, arrStr);
            return (jboolean) kv->set(value, key);
        } else {
            kv->removeValueForKey(key);
            return (jboolean) true;
        }
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeSet_2(JNIEnv *env, jobject, jlong handle, jstring oKey, jobjectArray arrStr, jint expiration) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (arrStr) {
            vector<string> value = jarray2vector(env, arrStr);
            return (jboolean) kv->set(value, key, (uint32_t) expiration);
        } else {
            kv->removeValueForKey(key);
            return (jboolean) true;
        }
    }
    return (jboolean) false;
}

MMKV_JNI jobjectArray decodeStringSet(JNIEnv *env, jobject, jlong handle, jstring oKey) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        vector<string> value;
        bool hasValue = kv->getVector(key, value);
        if (hasValue) {
            return vector2jarray(env, value);
        }
    }
    return nullptr;
}

MMKV_JNI void clearMemoryCache(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->clearMemoryCache();
    }
}

MMKV_JNI void lock(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->lock();
    }
}

MMKV_JNI void unlock(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->unlock();
    }
}

MMKV_JNI jboolean tryLock(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return (jboolean) kv->try_lock();
    }
    return jboolean(false);
}

MMKV_JNI jint pageSize(JNIEnv *env, jclass type) {
    return DEFAULT_MMAP_SIZE;
}

MMKV_JNI jstring version(JNIEnv *env, jclass type) {
    return string2jstring(env, MMKV_VERSION);
}

#    ifndef MMKV_DISABLE_CRYPT

MMKV_JNI jstring cryptKey(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        string cryptKey = kv->cryptKey();
        if (cryptKey.length() > 0) {
            return string2jstring(env, cryptKey);
        }
    }
    return nullptr;
}

MMKV_JNI jboolean reKey(JNIEnv *env, jobject instance, jstring cryptKey) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        string newKey;
        if (cryptKey) {
            newKey = jstring2string(env, cryptKey);
        }
        return (jboolean) kv->reKey(newKey);
    }
    return (jboolean) false;
}

MMKV_JNI void checkReSetCryptKey(JNIEnv *env, jobject instance, jstring cryptKey) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        string newKey;
        if (cryptKey) {
            newKey = jstring2string(env, cryptKey);
        }

        if (!cryptKey || newKey.empty()) {
            kv->checkReSetCryptKey(nullptr);
        } else {
            kv->checkReSetCryptKey(&newKey);
        }
    }
}

#    else

MMKV_JNI jstring cryptKey(JNIEnv *env, jobject instance) {
    return nullptr;
}

#    endif // MMKV_DISABLE_CRYPT

MMKV_JNI void trim(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->trim();
    }
}

MMKV_JNI void close(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->close();
        env->SetLongField(instance, g_fileID, 0);
    }
}

MMKV_JNI jint valueSize(JNIEnv *env, jobject, jlong handle, jstring oKey, jboolean actualSize) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return static_cast<jint>(kv->getValueSize(key, (bool) actualSize));
    }
    return 0;
}

MMKV_JNI void setLogLevel(JNIEnv *env, jclass type, jint level) {
    MMKV::setLogLevel((MMKVLogLevel) level);
}

MMKV_JNI void setCallbackHandler(JNIEnv *env, jclass type, jboolean logReDirecting, jboolean hasCallback) {
    if (logReDirecting == JNI_TRUE) {
        MMKV::registerLogHandler(mmkvLog);
    } else {
        MMKV::unRegisterLogHandler();
    }

    if (hasCallback == JNI_TRUE) {
        MMKV::registerErrorHandler(onMMKVError);
    } else {
        MMKV::unRegisterErrorHandler();
    }
}

MMKV_JNI jlong createNB(JNIEnv *env, jobject instance, jint size) {
    auto ptr = malloc(static_cast<size_t>(size));
    if (!ptr) {
        MMKVError("fail to create NativeBuffer:%s", strerror(errno));
        return 0;
    }
    return reinterpret_cast<jlong>(ptr);
}

MMKV_JNI void destroyNB(JNIEnv *env, jobject instance, jlong pointer, jint size) {
    free(reinterpret_cast<void *>(pointer));
}

MMKV_JNI jint writeValueToNB(JNIEnv *env, jobject instance, jlong handle, jstring oKey, jlong pointer, jint size) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return kv->writeValueToBuffer(key, reinterpret_cast<void *>(pointer), size);
    }
    return -1;
}

MMKV_JNI void setWantsContentChangeNotify(JNIEnv *env, jclass type, jboolean notify) {
    if (notify == JNI_TRUE) {
        MMKV::registerContentChangeHandler(onContentChangedByOuterProcess);
    } else {
        MMKV::unRegisterContentChangeHandler();
    }
}

MMKV_JNI void checkContentChanged(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->checkContentChanged();
    }
}

MMKV_JNI jboolean backupOne(JNIEnv *env, jobject obj, jstring mmapID, jstring dstDir, jstring rootPath) {
    if (rootPath) {
        string root = jstring2string(env, rootPath);
        if (root.length() > 0) {
            return (jboolean) MMKV::backupOneToDirectory(jstring2string(env, mmapID), jstring2string(env, dstDir), &root);
        }
    }
    return (jboolean) MMKV::backupOneToDirectory(jstring2string(env, mmapID), jstring2string(env, dstDir));
}

MMKV_JNI jboolean restoreOne(JNIEnv *env, jobject obj, jstring mmapID, jstring srcDir, jstring rootPath) {
    if (rootPath) {
        string root = jstring2string(env, rootPath);
        if (root.length() > 0) {
            return (jboolean) MMKV::restoreOneFromDirectory(jstring2string(env, mmapID), jstring2string(env, srcDir), &root);
        }
    }
    return (jboolean) MMKV::restoreOneFromDirectory(jstring2string(env, mmapID), jstring2string(env, srcDir));
}

MMKV_JNI jlong backupAll(JNIEnv *env, jobject obj, jstring dstDir/*, jstring rootPath*/) {
    // historically Android mistakenly use mmapKey as mmapID
    // makes everything tricky with customize root
    /*if (rootPath) {
        string root = jstring2string(env, rootPath);
        if (root.length() > 0) {
            return (jlong) MMKV::backupAllToDirectory(jstring2string(env, dstDir), &root);
        }
    }*/
    return (jlong) MMKV::backupAllToDirectory(jstring2string(env, dstDir));
}

MMKV_JNI jlong restoreAll(JNIEnv *env, jobject obj, jstring srcDir/*, jstring rootPath*/) {
    // historically Android mistakenly use mmapKey as mmapID
    // makes everything tricky with customize root
    /*if (rootPath) {
        string root = jstring2string(env, rootPath);
        if (root.length() > 0) {
            return (jlong) MMKV::restoreAllFromDirectory(jstring2string(env, srcDir), &root);
        }
    }*/
    return (jlong) MMKV::restoreAllFromDirectory(jstring2string(env, srcDir));
}

MMKV_JNI jboolean enableAutoExpire(JNIEnv *env, jobject instance, jint expireDuration) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return (jboolean) kv->enableAutoKeyExpire(expireDuration);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean disableAutoExpire(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return (jboolean) kv->disableAutoKeyExpire();
    }
    return (jboolean) false;
}

MMKV_JNI void enableCompareBeforeSet(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->enableCompareBeforeSet();
    }
}

MMKV_JNI void disableCompareBeforeSet(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->disableCompareBeforeSet();
    }
}

MMKV_JNI bool isCompareBeforeSetEnabled(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return kv->isCompareBeforeSetEnabled();
    }
    return false;
}

MMKV_JNI bool isEncryptionEnabled(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return kv->isEncryptionEnabled();
    }
    return false;
}

MMKV_JNI bool isExpirationEnabled(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return kv->isExpirationEnabled();
    }
    return false;
}

MMKV_JNI void clearAllWithKeepingSpace(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->clearAll(true);
    }
}

} // namespace mmkv

static JNINativeMethod g_methods[] = {
    {"onExit", "()V", (void *) mmkv::onExit},
    {"cryptKey", "()Ljava/lang/String;", (void *) mmkv::cryptKey},
#    ifndef MMKV_DISABLE_CRYPT
    {"reKey", "(Ljava/lang/String;)Z", (void *) mmkv::reKey},
    {"checkReSetCryptKey", "(Ljava/lang/String;)V", (void *) mmkv::checkReSetCryptKey},
#    endif
    {"pageSize", "()I", (void *) mmkv::pageSize},
    {"mmapID", "()Ljava/lang/String;", (void *) mmkv::mmapID},
    {"version", "()Ljava/lang/String;", (void *) mmkv::version},
    {"lock", "()V", (void *) mmkv::lock},
    {"unlock", "()V", (void *) mmkv::unlock},
    {"tryLock", "()Z", (void *) mmkv::tryLock},
    {"allKeys", "(JZ)[Ljava/lang/String;", (void *) mmkv::allKeys},
    {"removeValuesForKeys", "([Ljava/lang/String;)V", (void *) mmkv::removeValuesForKeys},
    {"clearAll", "()V", (void *) mmkv::clearAll},
    {"trim", "()V", (void *) mmkv::trim},
    {"close", "()V", (void *) mmkv::close},
    {"clearMemoryCache", "()V", (void *) mmkv::clearMemoryCache},
    {"sync", "(Z)V", (void *) mmkv::sync},
    {"isFileValid", "(Ljava/lang/String;Ljava/lang/String;)Z", (void *) mmkv::isFileValid},
    {"removeStorage", "(Ljava/lang/String;Ljava/lang/String;)Z", (void *) mmkv::removeStorage},
    {"ashmemFD", "()I", (void *) mmkv::ashmemFD},
    {"ashmemMetaFD", "()I", (void *) mmkv::ashmemMetaFD},
    //{"jniInitialize", "(Ljava/lang/String;Ljava/lang/String;I)V", (void *) mmkv::jniInitialize},
    {"jniInitialize", "(Ljava/lang/String;Ljava/lang/String;IZ)V", (void *) mmkv::jniInitialize_2},
    {"getMMKVWithID", "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;J)J", (void *) mmkv::getMMKVWithID},
    {"getMMKVWithIDAndSize", "(Ljava/lang/String;IILjava/lang/String;)J", (void *) mmkv::getMMKVWithIDAndSize},
    {"getDefaultMMKV", "(ILjava/lang/String;)J", (void *) mmkv::getDefaultMMKV},
    {"getMMKVWithAshmemFD", "(Ljava/lang/String;IILjava/lang/String;)J", (void *) mmkv::getMMKVWithAshmemFD},
    {"encodeBool", "(JLjava/lang/String;Z)Z", (void *) mmkv::encodeBool},
    {"encodeBool_2", "(JLjava/lang/String;ZI)Z", (void *) mmkv::encodeBool_2},
    {"decodeBool", "(JLjava/lang/String;Z)Z", (void *) mmkv::decodeBool},
    {"encodeInt", "(JLjava/lang/String;I)Z", (void *) mmkv::encodeInt},
    {"encodeInt_2", "(JLjava/lang/String;II)Z", (void *) mmkv::encodeInt_2},
    {"decodeInt", "(JLjava/lang/String;I)I", (void *) mmkv::decodeInt},
    {"encodeLong", "(JLjava/lang/String;J)Z", (void *) mmkv::encodeLong},
    {"encodeLong_2", "(JLjava/lang/String;JI)Z", (void *) mmkv::encodeLong_2},
    {"decodeLong", "(JLjava/lang/String;J)J", (void *) mmkv::decodeLong},
    {"encodeFloat", "(JLjava/lang/String;F)Z", (void *) mmkv::encodeFloat},
    {"encodeFloat_2", "(JLjava/lang/String;FI)Z", (void *) mmkv::encodeFloat_2},
    {"decodeFloat", "(JLjava/lang/String;F)F", (void *) mmkv::decodeFloat},
    {"encodeDouble", "(JLjava/lang/String;D)Z", (void *) mmkv::encodeDouble},
    {"encodeDouble_2", "(JLjava/lang/String;DI)Z", (void *) mmkv::encodeDouble_2},
    {"decodeDouble", "(JLjava/lang/String;D)D", (void *) mmkv::decodeDouble},
    {"encodeString", "(JLjava/lang/String;Ljava/lang/String;)Z", (void *) mmkv::encodeString},
    {"encodeString_2", "(JLjava/lang/String;Ljava/lang/String;I)Z", (void *) mmkv::encodeString_2},
    {"decodeString", "(JLjava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void *) mmkv::decodeString},
    {"encodeSet", "(JLjava/lang/String;[Ljava/lang/String;)Z", (void *) mmkv::encodeSet},
    {"encodeSet_2", "(JLjava/lang/String;[Ljava/lang/String;I)Z", (void *) mmkv::encodeSet_2},
    {"decodeStringSet", "(JLjava/lang/String;)[Ljava/lang/String;", (void *) mmkv::decodeStringSet},
    {"encodeBytes", "(JLjava/lang/String;[B)Z", (void *) mmkv::encodeBytes},
    {"encodeBytes_2", "(JLjava/lang/String;[BI)Z", (void *) mmkv::encodeBytes_2},
    {"decodeBytes", "(JLjava/lang/String;)[B", (void *) mmkv::decodeBytes},
    {"containsKey", "(JLjava/lang/String;)Z", (void *) mmkv::containsKey},
    {"count", "(JZ)J", (void *) mmkv::count},
    {"totalSize", "(J)J", (void *) mmkv::totalSize},
    {"actualSize", "(J)J", (void *) mmkv::actualSize},
    {"removeValueForKey", "(JLjava/lang/String;)V", (void *) mmkv::removeValueForKey},
    {"valueSize", "(JLjava/lang/String;Z)I", (void *) mmkv::valueSize},
    {"setLogLevel", "(I)V", (void *) mmkv::setLogLevel},
    {"setCallbackHandler", "(ZZ)V", (void *) mmkv::setCallbackHandler},
    {"createNB", "(I)J", (void *) mmkv::createNB},
    {"destroyNB", "(JI)V", (void *) mmkv::destroyNB},
    {"writeValueToNB", "(JLjava/lang/String;JI)I", (void *) mmkv::writeValueToNB},
    {"setWantsContentChangeNotify", "(Z)V", (void *) mmkv::setWantsContentChangeNotify},
    {"checkContentChangedByOuterProcess", "()V", (void *) mmkv::checkContentChanged},
    {"checkProcessMode", "(J)Z", (void *) mmkv::checkProcessMode},
    {"backupOneToDirectory", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z", (void *) mmkv::backupOne},
    {"restoreOneMMKVFromDirectory", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z", (void *) mmkv::restoreOne},
    {"backupAllToDirectory", "(Ljava/lang/String;)J", (void *) mmkv::backupAll},
    {"restoreAllFromDirectory", "(Ljava/lang/String;)J", (void *) mmkv::restoreAll},
    {"enableAutoKeyExpire", "(I)Z", (void *) mmkv::enableAutoExpire},
    {"disableAutoKeyExpire", "()Z", (void *) mmkv::disableAutoExpire},
    {"nativeEnableCompareBeforeSet", "()V", (void *) mmkv::enableCompareBeforeSet},
    {"disableCompareBeforeSet", "()V", (void *) mmkv::disableCompareBeforeSet},
    {"isCompareBeforeSetEnabled", "()Z", (void *) mmkv::isCompareBeforeSetEnabled},
    {"isEncryptionEnabled", "()Z", (void *) mmkv::isEncryptionEnabled},
    {"isExpirationEnabled", "()Z", (void *) mmkv::isExpirationEnabled},
    {"clearAllWithKeepingSpace", "()V", (void *) mmkv::clearAllWithKeepingSpace},
};

static int registerNativeMethods(JNIEnv *env, jclass cls) {
    return env->RegisterNatives(cls, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
}

#endif // MMKV_ANDROID
