#include <jni.h>
#include <string>
#include <cstdint>
#include "MMKV.h"
#include "MMKVLog.h"
#include "MMBuffer.h"

using namespace std;

string getFilesDir(JNIEnv *env, jobject obj) {
    jclass cls = env->GetObjectClass(obj);
    jmethodID getFilesDir = env->GetMethodID(cls, "getFilesDir", "()Ljava/io/File;");
    jobject dirObj = env->CallObjectMethod(obj,getFilesDir);
    jclass dir = env->GetObjectClass(dirObj);
    jmethodID getStoragePath = env->GetMethodID(dir, "getAbsolutePath", "()Ljava/lang/String;");
    jstring path = (jstring)env->CallObjectMethod(dirObj, getStoragePath);
    const char *pathStr = env->GetStringUTFChars(path, 0);
    string result(pathStr);
    env->ReleaseStringUTFChars(path, pathStr);
    return result;
}

void testMMKV(JNIEnv *env, jobject obj) {
    string root = getFilesDir(env, obj);
    MMKV::initializeMMKV(root + "/mmkv");
    MMKV* kv = MMKV::defaultMMKV();

//    kv->setInt32(__LINE__, "int");
    MMKVInfo("%d", kv->getInt32ForKey("int"));

//    kv->setInt64(std::numeric_limits<int64_t>::min(), "int64");
    MMKVInfo("int64:%ld", kv->getInt64ForKey("int64"));

//    kv->setUInt64(std::numeric_limits<uint64_t>::max(), "uint64");
    MMKVInfo("uint64:%lu", kv->getInt64ForKey("uint64"));

//    kv->setFloat(-3.1415926f, "float");
    MMKVInfo("float:%f", kv->getFloatForKey("float"));

//    kv->setDouble(std::numeric_limits<double>::max(), "double");
    MMKVInfo("double:%f", kv->getDoubleForKey("double"));

//    kv->setStringForKey(__FILE__, "string");
    string str;
    kv->getStringForKey("string", str);
    MMKVInfo("%s", str.c_str());
}

extern "C" JNIEXPORT JNICALL
jstring Java_com_tencent_mmkv_MainActivity_stringFromJNI(JNIEnv *env, jobject obj) {
//    testPBCoder();
//    testMMKV(env, obj);
    string root = getFilesDir(env, obj);
    return env->NewStringUTF(root.c_str());
}

extern "C" JNIEXPORT JNICALL
void Java_com_tencent_mmkv_MMKV_initialize(JNIEnv *env, jobject obj, jstring rootDir) {
    const char *kstr = env->GetStringUTFChars(rootDir, nullptr);
    MMKV::initializeMMKV(kstr);
    env->ReleaseStringUTFChars(rootDir, kstr);
}

extern "C" JNIEXPORT JNICALL
void Java_com_tencent_mmkv_MMKV_onExit(JNIEnv *env, jobject obj) {
    MMKV::onExit();
}

static MMKV* getMMKV(JNIEnv *env, jobject obj) {
    jclass c = env->GetObjectClass(obj);
    // J is the type signature for long:
    jfieldID fileID = env->GetFieldID(c, "nativeHandle", "J");
    jlong handle = env->GetLongField(obj, fileID);
    return reinterpret_cast<MMKV *>(handle);
}

static string jstring2string(JNIEnv* env, jstring str) {
    const char *kstr = env->GetStringUTFChars(str, nullptr);
    string result(kstr);
    env->ReleaseStringUTFChars(str, kstr);
    return result;
}

static jstring string2jstring(JNIEnv* env, const string& str) {
    return env->NewStringUTF(str.c_str());
}

static vector<string> jarray2vector(JNIEnv* env, jobjectArray array) {
    jsize size = env->GetArrayLength(array);
    vector<string> keys;
    keys.reserve(size);
    for (jsize i = 0; i < size; i++) {
        jstring str = (jstring) env->GetObjectArrayElement(array, i);
        keys.push_back(jstring2string(env, str));
    }
    return keys;
}

static jobjectArray vector2jarray(JNIEnv* env, const vector<string>& arr) {
    if (!arr.empty()) {
        jobjectArray result = env->NewObjectArray(arr.size(), env->FindClass("java/lang/String"), nullptr);
        for (size_t index = 0; index < arr.size(); index++) {
            env->SetObjectArrayElement(result, index, string2jstring(env, arr[index]));
        }
        return result;
    }
    return nullptr;
}

extern "C" JNIEXPORT JNICALL
jlong Java_com_tencent_mmkv_MMKV_getMMKVWithID(JNIEnv *env, jobject obj, jstring mmapID) {
    string str = jstring2string(env, mmapID);
    MMKV* kv = MMKV::mmkvWithID(str);
    return (jlong)kv;
}

extern "C" JNIEXPORT JNICALL
jlong Java_com_tencent_mmkv_MMKV_getDefaultMMKV(JNIEnv *env, jobject obj) {
    MMKV* kv = MMKV::defaultMMKV();
    return (jlong)kv;
}

extern "C" JNIEXPORT JNICALL
jstring Java_com_tencent_mmkv_MMKV_mmapID(JNIEnv *env, jobject instance) {
    MMKV* kv = getMMKV(env, instance);
    if (kv) {
        return string2jstring(env, kv->mmapID());
    }
    return nullptr;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeBool(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jboolean value) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jboolean)kv->setBool(value, key);
    }
    return (jboolean)false;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_decodeBool(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jboolean defaultValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jboolean)kv->getBoolForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeInt(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint value) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jboolean)kv->setInt32(value, key);
    }
    return (jboolean)false;
}

extern "C" JNIEXPORT JNICALL
jint Java_com_tencent_mmkv_MMKV_decodeInt(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jint defaultValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jint)kv->getInt32ForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeLong(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong value) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jboolean)kv->setInt64(value, key);
    }
    return (jboolean)false;
}

extern "C" JNIEXPORT JNICALL
jlong Java_com_tencent_mmkv_MMKV_decodeLong(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jlong defaultValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jlong)kv->getInt64ForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeFloat(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jfloat value) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jboolean)kv->setFloat(value, key);
    }
    return (jboolean)false;
}

extern "C" JNIEXPORT JNICALL
jfloat Java_com_tencent_mmkv_MMKV_decodeFloat(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jfloat defaultValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jfloat)kv->getFloatForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeDouble(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jdouble value) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jboolean)kv->setDouble(value, key);
    }
    return (jboolean)false;
}

extern "C" JNIEXPORT JNICALL
jdouble Java_com_tencent_mmkv_MMKV_decodeDouble(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jdouble defaultValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jdouble)kv->getDoubleForKey(key, defaultValue);
    }
    return defaultValue;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeString(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jstring oValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        string value = jstring2string(env, oValue);
        return (jboolean)kv->setStringForKey(value, key);
    }
    return (jboolean)false;
}

extern "C" JNIEXPORT JNICALL
jstring Java_com_tencent_mmkv_MMKV_decodeString(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jstring oDefaultValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        string value;
        bool hasValue = kv->getStringForKey(key, value);
        if (hasValue) {
            return string2jstring(env, value);
        }
    }
    return oDefaultValue;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeBytes(JNIEnv *env, jobject obj, jlong handle, jstring oKey, jbyteArray oValue) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        MMBuffer value(0);
        {
            jsize len = env->GetArrayLength(oValue);
            void * bufferPtr = env->GetPrimitiveArrayCritical(oValue, NULL);
            if (bufferPtr) {
                value = MMBuffer(bufferPtr, len);
                env->ReleasePrimitiveArrayCritical(oValue, bufferPtr, JNI_ABORT);
            } else {
                MMKVError("fail to get array: %s=%p", key.c_str(), oValue);
            }
        }
        return (jboolean)kv->setBytesForKey(value, key);
    }
    return (jboolean)false;
}

extern "C" JNIEXPORT JNICALL
jbyteArray Java_com_tencent_mmkv_MMKV_decodeBytes(JNIEnv *env, jobject obj, jlong handle, jstring oKey) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        MMBuffer value = kv->getBytesForKey(key);
        jbyteArray result = env->NewByteArray(value.length());
        if (result) {
            env->SetByteArrayRegion(result, 0, value.length(), (const jbyte*)value.getPtr());
            return result;
        }
    }
    return nullptr;
}

extern "C" JNIEXPORT JNICALL
jobjectArray Java_com_tencent_mmkv_MMKV_allKeys(JNIEnv *env, jobject instance) {
    MMKV* kv = getMMKV(env, instance);
    if (kv) {
        vector<string> keys = kv->allKeys();
        return vector2jarray(env, keys);
    }
    return nullptr;
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_containsKey(JNIEnv *env, jobject instance, jlong handle, jstring oKey) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        return (jboolean)kv->containsKey(key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL
jlong Java_com_tencent_mmkv_MMKV_count(JNIEnv *env, jobject instance, jlong handle) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->count();
        return size;
    }
    return 0;
}

extern "C" JNIEXPORT JNICALL
jlong Java_com_tencent_mmkv_MMKV_totalSize(JNIEnv *env, jobject instance, jlong handle) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        jlong size = kv->totalSize();
        return size;
    }
    return 0;
}

extern "C" JNIEXPORT JNICALL
void Java_com_tencent_mmkv_MMKV_removeValueForKey(JNIEnv *env, jobject instance, jlong handle, jstring oKey) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        kv->removeValueForKey(key);
    }
}

extern "C" JNIEXPORT JNICALL
void Java_com_tencent_mmkv_MMKV_removeValuesForKeys(JNIEnv *env, jobject instance, jobjectArray arrKeys) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        vector<string> keys = jarray2vector(env, arrKeys);
        if (!keys.empty()) {
            kv->removeValuesForKeys(keys);
        }
    }
}

extern "C" JNIEXPORT JNICALL
void Java_com_tencent_mmkv_MMKV_clearAll(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->clearAll();
    }
}

extern "C" JNIEXPORT JNICALL
void Java_com_tencent_mmkv_MMKV_sync(JNIEnv *env, jobject instance) {
    MMKV *kv = getMMKV(env, instance);
    if (kv) {
        kv->sync();
    }
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_isFileValid(JNIEnv *env, jclass type, jstring oMmapID) {
    string mmapID = jstring2string(env, oMmapID);
    return (jboolean)MMKV::isFileValid(mmapID);
}

extern "C" JNIEXPORT JNICALL
jboolean Java_com_tencent_mmkv_MMKV_encodeSet(JNIEnv *env, jobject instance, jlong handle, jstring oKey, jobjectArray arrStr) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        vector<string> value = jarray2vector(env, arrStr);
        return (jboolean)kv->setVectorForKey(value, key);
    }
    return (jboolean) false;
}

extern "C" JNIEXPORT JNICALL
jobjectArray Java_com_tencent_mmkv_MMKV_decodeStringSet(JNIEnv *env, jobject instance, jlong handle, jstring oKey) {
    MMKV* kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        string key = jstring2string(env, oKey);
        vector<string> value;
        bool hasValue = kv->getVectorForKey(key, value);
        if (hasValue) {
            return vector2jarray(env, value);
        }
    }
    return nullptr;
}