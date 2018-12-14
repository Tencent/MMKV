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
static JavaVM *g_currentJVM = nullptr;

extern "C" JNIEXPORT JNICALL jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    g_currentJVM = vm;
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    // Get jclass with env->FindClass.
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
    g_fileID = env->GetFieldID(g_cls, "nativeHandle", "J");
    if (!g_cls || !g_fileID) {
        MMKVError("fail to locate fileID");
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

    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT JNICALL void
Java_com_tencent_mmkv_MMKV_initialize(JNIEnv *env, jobject obj, jstring rootDir) {
    if (!rootDir) {
        return;
    }
    const char *kstr = env->GetStringUTFChars(rootDir, nullptr);
    if (kstr) {
        MMKV::initializeMMKV(kstr);
        env->ReleaseStringUTFChars(rootDir, kstr);
    }
}

extern "C" JNIEXPORT JNICALL void Java_com_tencent_mmkv_MMKV_onExit(JNIEnv *env, jobject obj) {
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
    if (g_currentJVM && g_callbackOnCRCFailID) {
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

extern "C" JNIEXPORT JNICALL jlong Java_com_tencent_mmkv_MMKV_getMMKVWithID(
    JNIEnv *env, jobject obj, jstring mmapID, jint mode, jstring cryptKey) {
    MMKV *kv = nullptr;
    if (!mmapID) {
        return (jlong) kv;
    }
    string str = jstring2string(env, mmapID);

    if (cryptKey != nullptr) {
        string crypt = jstring2string(env, cryptKey);
        if (crypt.length() > 0) {
            kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, &crypt);
        }
    }
    if (!kv) {
        kv = MMKV::mmkvWithID(str, DEFAULT_MMAP_SIZE, (MMKVMode) mode, nullptr);
    }

    return (jlong) kv;
}

extern "C" JNIEXPORT JNICALL jlong Java_com_tencent_mmkv_MMKV_getMMKVWithIDAndSize(
    JNIEnv *env, jobject obj, jstring mmapID, jint size, jint mode, jstring cryptKey) {
    MMKV *kv = nullptr;
    if (!mmapID || size < 0) {
        return (jlong) kv;
    }
    string str = jstring2string(env, mmapID);

    if (cryptKey != nullptr) {
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

extern "C" JNIEXPORT JNICALL jlong Java_com_tencent_mmkv_MMKV_getDefaultMMKV(JNIEnv *env,
                                                                             jobject obj,
                                                                             jint mode,
                                                                             jstring cryptKey) {
    MMKV *kv = nullptr;

    if (cryptKey != nullptr) {
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

extern "C" JNIEXPORT JNICALL jlong Java_com_tencent_mmkv_MMKV_getMMKVWithAshmemFD(
    JNIEnv *env, jobject obj, jstring mmapID, jint fd, jint metaFD, jstring cryptKey) {
    MMKV *kv = nullptr;
    if (!mmapID || fd < 0 || metaFD < 0) {
        return (jlong) kv;
    }
    string id = jstring2string(env, mmapID);

    if (cryptKey != nullptr) {
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

extern "C" JNIEXPORT JNICALL jstring Java_com_tencent_mmkv_MMKV_mmapID(JNIEnv *env,
                                                                       jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return string2jstring(env, kv->mmapID());
    }
    return nullptr;
}

extern "C" JNIEXPORT JNICALL jint Java_com_tencent_mmkv_MMKV_ashmemFD(JNIEnv *env,
                                                                      jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return kv->ashmemFD();
    }
    return -1;
}

extern "C" JNIEXPORT JNICALL jint Java_com_tencent_mmkv_MMKV_ashmemMetaFD(JNIEnv *env,
                                                                          jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return kv->ashmemMetaFD();
    }
    return -1;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeBool(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jboolean value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setBool(value, key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_decodeBool(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jboolean defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->getBoolForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeInt(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setInt32(value, key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL jint Java_com_tencent_mmkv_MMKV_decodeInt(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jint) kv->getInt32ForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeLong(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setInt64(value, key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL jlong Java_com_tencent_mmkv_MMKV_decodeLong(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jlong) kv->getInt64ForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeFloat(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jfloat value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setFloat(value, key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL jfloat Java_com_tencent_mmkv_MMKV_decodeFloat(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jfloat defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jfloat) kv->getFloatForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeDouble(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jdouble value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->setDouble(value, key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL jdouble Java_com_tencent_mmkv_MMKV_decodeDouble(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jdouble defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jdouble) kv->getDoubleForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeString(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jstring oValue) {
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

extern "C" JNIEXPORT JNICALL jstring Java_com_tencent_mmkv_MMKV_decodeString(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jstring oDefaultValue) {
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

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeBytes(
    JNIEnv *env, jobject obj, jlong handle, jstring oKey, jbyteArray oValue) {
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

extern "C" JNIEXPORT JNICALL jbyteArray Java_com_tencent_mmkv_MMKV_decodeBytes(JNIEnv *env,
                                                                               jobject obj,
                                                                               jlong handle,
                                                                               jstring oKey) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        MMBuffer value = kv->getBytesForKey(key);
        jbyteArray result = env->NewByteArray(value.length());
        if (result) {
            env->SetByteArrayRegion(result, 0, value.length(), (const jbyte *) value.getPtr());
            return result;
        }
    }
    return nullptr;
}

extern "C" JNIEXPORT JNICALL jobjectArray Java_com_tencent_mmkv_MMKV_allKeys(JNIEnv *env,
                                                                             jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        vector<string> keys = kv->allKeys();
        return vector2jarray(env, keys);
    }
    return nullptr;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_containsKey(JNIEnv *env,
                                                                             jobject instance,
                                                                             jlong handle,
                                                                             jstring oKey) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        return (jboolean) kv->containsKey(key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL jlong Java_com_tencent_mmkv_MMKV_count(JNIEnv *env,
                                                                    jobject instance,
                                                                    jlong handle) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->count();
        return size;
    }
    return 0;
}

extern "C" JNIEXPORT JNICALL jlong Java_com_tencent_mmkv_MMKV_totalSize(JNIEnv *env,
                                                                        jobject instance,
                                                                        jlong handle) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->totalSize();
        return size;
    }
    return 0;
}

extern "C" JNIEXPORT JNICALL void Java_com_tencent_mmkv_MMKV_removeValueForKey(JNIEnv *env,
                                                                               jobject instance,
                                                                               jlong handle,
                                                                               jstring oKey) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        string key = jstring2string(env, oKey);
        kv->removeValueForKey(key);
    }
}

extern "C" JNIEXPORT JNICALL void Java_com_tencent_mmkv_MMKV_removeValuesForKeys(
    JNIEnv *env, jobject instance, jobjectArray arrKeys) {
    MMKV *kv = getMMKV(env, instance);
    if (kv && arrKeys) {
        vector<string> keys = jarray2vector(env, arrKeys);
        if (!keys.empty()) {
            kv->removeValuesForKeys(keys);
        }
    }
}

extern "C" JNIEXPORT JNICALL void Java_com_tencent_mmkv_MMKV_clearAll(JNIEnv *env,
                                                                      jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->clearAll();
    }
}

extern "C" JNIEXPORT JNICALL void Java_com_tencent_mmkv_MMKV_sync(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->sync();
    }
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_isFileValid(JNIEnv *env,
                                                                             jclass type,
                                                                             jstring oMmapID) {
    if (oMmapID) {
        string mmapID = jstring2string(env, oMmapID);
        return (jboolean) MMKV::isFileValid(mmapID);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL jboolean Java_com_tencent_mmkv_MMKV_encodeSet(
    JNIEnv *env, jobject instance, jlong handle, jstring oKey, jobjectArray arrStr) {
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

extern "C" JNIEXPORT JNICALL jobjectArray Java_com_tencent_mmkv_MMKV_decodeStringSet(
    JNIEnv *env, jobject instance, jlong handle, jstring oKey) {
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

extern "C" JNIEXPORT JNICALL void Java_com_tencent_mmkv_MMKV_clearMemoryCache(JNIEnv *env,
                                                                              jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->clearMemoryState();
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_tencent_mmkv_MMKV_lock(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->lock();
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_tencent_mmkv_MMKV_unlock(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->unlock();
    }
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_tencent_mmkv_MMKV_tryLock(JNIEnv *env,
                                                                         jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        return (jboolean) kv->try_lock();
    }
    return jboolean(false);
}

extern "C" JNIEXPORT jint JNICALL Java_com_tencent_mmkv_MMKV_pageSize(JNIEnv *env, jclass type) {
    return DEFAULT_MMAP_SIZE;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_tencent_mmkv_MMKV_cryptKey(JNIEnv *env,
                                                                         jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        string cryptKey = kv->cryptKey();
        if (cryptKey.length() > 0) {
            return string2jstring(env, cryptKey);
        }
    }
    return nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_tencent_mmkv_MMKV_reKey(JNIEnv *env,
                                                                       jobject instance,
                                                                       jstring cryptKey) {
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

extern "C" JNIEXPORT void JNICALL Java_com_tencent_mmkv_MMKV_checkReSetCryptKey(JNIEnv *env,
                                                                                jobject instance,
                                                                                jstring cryptKey) {
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

extern "C" JNIEXPORT void JNICALL Java_com_tencent_mmkv_MMKV_trim(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->trim();
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_tencent_mmkv_MMKV_close(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->close();
        env->SetLongField(instance, g_fileID, 0);
    }
}
