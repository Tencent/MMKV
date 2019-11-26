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

#include "native-bridge.h"
#include "MMBuffer.h"
#include "MMKV.h"
#include "MMKVLog.h"
#include <cstdint>
#include <jni.h>
#include <string>

using namespace std;

static jclass g_cls = nullptr;
static jfieldID g_fileID = nullptr;
static jmethodID g_callbackOnCRCFailID = nullptr;
static jmethodID g_callbackOnFileLengthErrorID = nullptr;
static jmethodID g_mmkvLogID = nullptr;
static jmethodID g_callbackOnContentChange = nullptr;
static JavaVM *g_currentJVM = nullptr;
int g_android_api = __ANDROID_API_L__;

static int registerNativeMethods(JNIEnv *env, jclass cls);

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
    int ret = registerNativeMethods(env, g_cls);
    if (ret != 0) {
        MMKVError("fail to register native methods for class %s, ret = %d", clsName, ret);
        return -4;
    }
    g_fileID = env->GetFieldID(g_cls, "nativeHandle", "J");
    if (!g_fileID) {
        MMKVError("fail to locate fileID");
        return -5;
    }

    g_callbackOnCRCFailID =
        env->GetStaticMethodID(g_cls, "onMMKVCRCCheckFail", "(Ljava/lang/String;)I");
    if (!g_callbackOnCRCFailID) {
        MMKVError("fail to get method id for onMMKVCRCCheckFail");
    }
    g_callbackOnFileLengthErrorID =
        env->GetStaticMethodID(g_cls, "onMMKVFileLengthError", "(Ljava/lang/String;)I");
    if (!g_callbackOnFileLengthErrorID) {
        MMKVError("fail to get method id for onMMKVFileLengthError");
    }
    g_mmkvLogID = env->GetStaticMethodID(
        g_cls, "mmkvLogImp", "(ILjava/lang/String;ILjava/lang/String;Ljava/lang/String;)V");
    if (!g_mmkvLogID) {
        MMKVError("fail to get method id for mmkvLogImp");
    }
    g_callbackOnContentChange =
        env->GetStaticMethodID(g_cls, "onContentChangedByOuterProcess", "(Ljava/lang/String;)V");
    if (!g_callbackOnContentChange) {
        MMKVError("fail to get method id for onContentChangedByOuterProcess()");
    }

    // get current API level by accessing android.os.Build.VERSION.SDK_INT
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (versionClass) {
        jfieldID sdkIntFieldID = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
        if (sdkIntFieldID) {
            g_android_api = env->GetStaticIntField(versionClass, sdkIntFieldID);
            MMKVInfo("current API level = %d", g_android_api);
        } else {
            MMKVError("fail to get field id android.os.Build.VERSION.SDK_INT");
        }
    } else {
        MMKVError("fail to get class android.os.Build.VERSION");
    }

    return JNI_VERSION_1_6;
}

//#define MMKV_JNI extern "C" JNIEXPORT JNICALL
#define MMKV_JNI static

namespace mmkv {

MMKV_JNI void jniInitialize(JNIEnv *env, jobject obj, jstring rootDir, jint logLevel) {
    if (!rootDir) {
        return;
    }
    const char *kstr = env->GetStringUTFChars(rootDir, nullptr);
    if (kstr) {
        g_currentLogLevel = (MMKVLogLevel) logLevel;

        MMKV::initializeMMKV(kstr);
        env->ReleaseStringUTFChars(rootDir, kstr);
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
    if (!arr.empty()) {
        jobjectArray result =
            env->NewObjectArray(arr.size(), env->FindClass("java/lang/String"), nullptr);
        if (result) {
            for (size_t index = 0; index < arr.size(); index++) {
                jstring value = string2jstring(env, arr[index]);
                env->SetObjectArrayElement(result, index, value);
                env->DeleteLocalRef(value);
            }
        }
        return result;
    }
    return nullptr;
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

MMKVRecoverStrategic onMMKVCRCCheckFail(const std::string &mmapID) {
    auto currentEnv = getCurrentEnv();
    if (currentEnv && g_callbackOnCRCFailID) {
        jstring str = string2jstring(currentEnv, mmapID);
        auto strategic = currentEnv->CallStaticIntMethod(g_cls, g_callbackOnCRCFailID, str);
        return static_cast<MMKVRecoverStrategic>(strategic);
    }
    return OnErrorDiscard;
}

MMKVRecoverStrategic onMMKVFileLengthError(const std::string &mmapID) {
    auto currentEnv = getCurrentEnv();
    if (currentEnv && g_callbackOnFileLengthErrorID) {
        jstring str = string2jstring(currentEnv, mmapID);
        auto strategic = currentEnv->CallStaticIntMethod(g_cls, g_callbackOnFileLengthErrorID, str);
        return static_cast<MMKVRecoverStrategic>(strategic);
    }
    return OnErrorDiscard;
}

void mmkvLog(int level,
             const std::string &file,
             int line,
             const std::string &function,
             const std::string &message) {
    auto currentEnv = getCurrentEnv();
    if (currentEnv && g_mmkvLogID) {
        jstring oFile = string2jstring(currentEnv, file);
        jstring oFunction = string2jstring(currentEnv, function);
        jstring oMessage = string2jstring(currentEnv, message);
        currentEnv->CallStaticVoidMethod(g_cls, g_mmkvLogID, level, oFile, line, oFunction,
                                         oMessage);
    }
}

void onContentChangedByOuterProcess(const std::string &mmapID) {
    auto currentEnv = getCurrentEnv();
    if (currentEnv && g_callbackOnContentChange) {
        jstring str = string2jstring(currentEnv, mmapID);
        currentEnv->CallStaticVoidMethod(g_cls, g_callbackOnContentChange, str);
    }
}

MMKV_JNI jlong getMMKVWithID(
    JNIEnv *env, jobject, jstring mmapID, jint mode, jstring cryptKey, jstring relativePath) {
    MMKV *kv = nullptr;
    if (!mmapID) {
        return (jlong) kv;
    }
    string str = jstring2string(env, mmapID);

    bool done = false;
    if (cryptKey) {
        string crypt = jstring2string(env, cryptKey);
        if (crypt.length() > 0) {
            if (relativePath) {
                string path = jstring2string(env, relativePath);
                kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, &crypt, &path);
            } else {
                kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, &crypt, nullptr);
            }
            done = true;
        }
    }
    if (!done) {
        if (relativePath) {
            string path = jstring2string(env, relativePath);
            kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, nullptr, &path);
        } else {
            kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, nullptr, nullptr);
        }
    }

    return (jlong) kv;
}

MMKV_JNI jlong getMMKVWithIDAndSize(
    JNIEnv *env, jobject obj, jstring mmapID, jint size, jint mode, jstring cryptKey) {
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

MMKV_JNI jlong getMMKVWithAshmemFD(
    JNIEnv *env, jobject obj, jstring mmapID, jint fd, jint metaFD, jstring cryptKey) {
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

MMKV_JNI jboolean encodeBool(JNIEnv *env, jobject, jlong handle, jstring oKey, jboolean value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setBool(value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean
decodeBool(JNIEnv *env, jobject, jlong handle, jstring oKey, jboolean defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->getBoolForKey(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeInt(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setInt32(value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jint decodeInt(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jint) kv->getInt32ForKey(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeLong(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setInt64(value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jlong
decodeLong(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jlong) kv->getInt64ForKey(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeFloat(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jfloat value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setFloat(value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jfloat decodeFloat(JNIEnv *env, jobject, jlong handle, jstring oKey, jfloat defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jfloat) kv->getFloatForKey(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean
encodeDouble(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jdouble value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setDouble(value, key);
    }
    return (jboolean) false;
}

MMKV_JNI jdouble
decodeDouble(JNIEnv *env, jobject, jlong handle, jstring oKey, jdouble defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jdouble) kv->getDoubleForKey(key, defaultValue);
    }
    return defaultValue;
}

MMKV_JNI jboolean encodeString(JNIEnv *env, jobject, jlong handle, jstring oKey, jstring oValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (oValue) {
            string value = jstring2string(env, oValue);
            return (jboolean) kv->setStringForKey(value, key);
        } else {
            kv->removeValueForKey(key);
            return (jboolean) true;
        }
    }
    return (jboolean) false;
}

MMKV_JNI jstring
decodeString(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jstring oDefaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        string value;
        bool hasValue = kv->getStringForKey(key, value);
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
            return (jboolean) kv->setBytesForKey(value, key);
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
        MMBuffer value = kv->getBytesForKey(key);
        if (value.length() > 0) {
            jbyteArray result = env->NewByteArray(value.length());
            env->SetByteArrayRegion(result, 0, value.length(), (const jbyte *) value.getPtr());
            return result;
        }
    }
    return nullptr;
}

MMKV_JNI jobjectArray allKeys(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        vector<string> keys = kv->allKeys();
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

MMKV_JNI jlong count(JNIEnv *env, jobject instance, jlong handle) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->count();
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
        kv->sync((bool) sync);
    }
}

MMKV_JNI jboolean isFileValid(JNIEnv *env, jclass type, jstring oMmapID) {
    if (oMmapID) {
        string mmapID = jstring2string(env, oMmapID);
        return (jboolean) MMKV::isFileValid(mmapID);
    }
    return (jboolean) false;
}

MMKV_JNI jboolean encodeSet(JNIEnv *env, jobject, jlong handle, jstring oKey, jobjectArray arrStr) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        if (arrStr) {
            vector<string> value = jarray2vector(env, arrStr);
            return (jboolean) kv->setVectorForKey(value, key);
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
        bool hasValue = kv->getVectorForKey(key, value);
        if (hasValue) {
            return vector2jarray(env, value);
        }
    }
    return nullptr;
}

MMKV_JNI void clearMemoryCache(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->clearMemoryState();
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
        return static_cast<jint>(kv->getValueSizeForKey(key, (bool) actualSize));
    }
    return 0;
}

MMKV_JNI void setLogLevel(JNIEnv *env, jclass type, jint level) {
    g_currentLogLevel = (MMKVLogLevel) level;
}

MMKV_JNI void setLogReDirecting(JNIEnv *env, jclass type, jboolean enable) {
    g_isLogRedirecting = (enable == JNI_TRUE);
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

MMKV_JNI jint writeValueToNB(
    JNIEnv *env, jobject instance, jlong handle, jstring oKey, jlong pointer, jint size) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return kv->writeValueToBuffer(key, reinterpret_cast<void *>(pointer), size);
    }
    return -1;
}

MMKV_JNI void setWantsContentChangeNotify(JNIEnv *env, jclass type, jboolean notify) {
    g_isContentChangeNotifying = (notify == JNI_TRUE);
}

MMKV_JNI void checkContentChanged(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->checkContentChanged();
    }
}

} // namespace mmkv

static JNINativeMethod g_methods[] = {
    {"onExit", "()V", (void *) mmkv::onExit},
    {"cryptKey", "()Ljava/lang/String;", (void *) mmkv::cryptKey},
    {"reKey", "(Ljava/lang/String;)Z", (void *) mmkv::reKey},
    {"checkReSetCryptKey", "(Ljava/lang/String;)V", (void *) mmkv::checkReSetCryptKey},
    {"pageSize", "()I", (void *) mmkv::pageSize},
    {"mmapID", "()Ljava/lang/String;", (void *) mmkv::mmapID},
    {"lock", "()V", (void *) mmkv::lock},
    {"unlock", "()V", (void *) mmkv::unlock},
    {"tryLock", "()Z", (void *) mmkv::tryLock},
    {"allKeys", "()[Ljava/lang/String;", (void *) mmkv::allKeys},
    {"removeValuesForKeys", "([Ljava/lang/String;)V", (void *) mmkv::removeValuesForKeys},
    {"clearAll", "()V", (void *) mmkv::clearAll},
    {"trim", "()V", (void *) mmkv::trim},
    {"close", "()V", (void *) mmkv::close},
    {"clearMemoryCache", "()V", (void *) mmkv::clearMemoryCache},
    {"sync", "(Z)V", (void *) mmkv::sync},
    {"isFileValid", "(Ljava/lang/String;)Z", (void *) mmkv::isFileValid},
    {"ashmemFD", "()I", (void *) mmkv::ashmemFD},
    {"ashmemMetaFD", "()I", (void *) mmkv::ashmemMetaFD},
    {"jniInitialize", "(Ljava/lang/String;I)V", (void *) mmkv::jniInitialize},
    {"getMMKVWithID", "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)J",
     (void *) mmkv::getMMKVWithID},
    {"getMMKVWithIDAndSize", "(Ljava/lang/String;IILjava/lang/String;)J",
     (void *) mmkv::getMMKVWithIDAndSize},
    {"getDefaultMMKV", "(ILjava/lang/String;)J", (void *) mmkv::getDefaultMMKV},
    {"getMMKVWithAshmemFD", "(Ljava/lang/String;IILjava/lang/String;)J",
     (void *) mmkv::getMMKVWithAshmemFD},
    {"encodeBool", "(JLjava/lang/String;Z)Z", (void *) mmkv::encodeBool},
    {"decodeBool", "(JLjava/lang/String;Z)Z", (void *) mmkv::decodeBool},
    {"encodeInt", "(JLjava/lang/String;I)Z", (void *) mmkv::encodeInt},
    {"decodeInt", "(JLjava/lang/String;I)I", (void *) mmkv::decodeInt},
    {"encodeLong", "(JLjava/lang/String;J)Z", (void *) mmkv::encodeLong},
    {"decodeLong", "(JLjava/lang/String;J)J", (void *) mmkv::decodeLong},
    {"encodeFloat", "(JLjava/lang/String;F)Z", (void *) mmkv::encodeFloat},
    {"decodeFloat", "(JLjava/lang/String;F)F", (void *) mmkv::decodeFloat},
    {"encodeDouble", "(JLjava/lang/String;D)Z", (void *) mmkv::encodeDouble},
    {"decodeDouble", "(JLjava/lang/String;D)D", (void *) mmkv::decodeDouble},
    {"encodeString", "(JLjava/lang/String;Ljava/lang/String;)Z", (void *) mmkv::encodeString},
    {"decodeString", "(JLjava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
     (void *) mmkv::decodeString},
    {"encodeSet", "(JLjava/lang/String;[Ljava/lang/String;)Z", (void *) mmkv::encodeSet},
    {"decodeStringSet", "(JLjava/lang/String;)[Ljava/lang/String;", (void *) mmkv::decodeStringSet},
    {"encodeBytes", "(JLjava/lang/String;[B)Z", (void *) mmkv::encodeBytes},
    {"decodeBytes", "(JLjava/lang/String;)[B", (void *) mmkv::decodeBytes},
    {"containsKey", "(JLjava/lang/String;)Z", (void *) mmkv::containsKey},
    {"count", "(J)J", (void *) mmkv::count},
    {"totalSize", "(J)J", (void *) mmkv::totalSize},
    {"removeValueForKey", "(JLjava/lang/String;)V", (void *) mmkv::removeValueForKey},
    {"valueSize", "(JLjava/lang/String;Z)I", (void *) mmkv::valueSize},
    {"setLogLevel", "(I)V", (void *) mmkv::setLogLevel},
    {"setLogReDirecting", "(Z)V", (void *) mmkv::setLogReDirecting},
    {"createNB", "(I)J", (void *) mmkv::createNB},
    {"destroyNB", "(JI)V", (void *) mmkv::destroyNB},
    {"writeValueToNB", "(JLjava/lang/String;JI)I", (void *) mmkv::writeValueToNB},
    {"setWantsContentChangeNotify", "(Z)V", (void *) mmkv::setWantsContentChangeNotify},
    {"checkContentChangedByOuterProcess", "()V", (void *) mmkv::checkContentChanged},
};

static int registerNativeMethods(JNIEnv *env, jclass cls) {
    return env->RegisterNatives(cls, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
}
