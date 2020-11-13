/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

#ifdef MMKV_POSIX

#    include "MMKV.h"
#    include <stdint.h>
#    include <string>

using namespace mmkv;
using namespace std;

#    define MMKV_EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))
#    define jstring const char *
#    define jint int32_t
#    define jlong int64_t
#    define jboolean bool

MMKV_EXPORT int32_t native_add(int32_t x, int32_t y) {
    return x + y;
}

MMKV_EXPORT void mmkvInitialize(const char *rootDir, int32_t logLevel) {
    if (!rootDir) {
        return;
    }
    MMKV::initializeMMKV(rootDir, (MMKVLogLevel) logLevel);
}

MMKV_EXPORT void onExit() {
    MMKV::onExit();
}

MMKV_EXPORT void *getMMKVWithID(jstring mmapID, /*jint mode,*/ jstring cryptKey, jstring rootPath) {
    MMKV *kv = nullptr;
    if (!mmapID) {
        return kv;
    }
    string str = mmapID;
    jint mode = MMKV_SINGLE_PROCESS;

    bool done = false;
    if (cryptKey) {
        string crypt = cryptKey;
        if (crypt.length() > 0) {
            if (rootPath) {
                string path = rootPath;
                kv = MMKV::mmkvWithID(str, (MMKVMode) mode, &crypt, &path);
            } else {
                kv = MMKV::mmkvWithID(str, (MMKVMode) mode, &crypt, nullptr);
            }
            done = true;
        }
    }
    if (!done) {
        if (rootPath) {
            string path = rootPath;
            kv = MMKV::mmkvWithID(str, (MMKVMode) mode, nullptr, &path);
        } else {
            kv = MMKV::mmkvWithID(str, (MMKVMode) mode, nullptr, nullptr);
        }
    }

    return kv;
}

MMKV_EXPORT void *getDefaultMMKV(jint mode, jstring cryptKey) {
    MMKV *kv = nullptr;

    if (cryptKey) {
        string crypt = cryptKey;
        if (crypt.length() > 0) {
            kv = MMKV::defaultMMKV((MMKVMode) mode, &crypt);
        }
    }
    if (!kv) {
        kv = MMKV::defaultMMKV((MMKVMode) mode, nullptr);
    }

    return kv;
}

MMKV_EXPORT jstring mmapID(const void *handle) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv) {
        return kv->mmapID().c_str();
    }
    return nullptr;
}

MMKV_EXPORT jboolean encodeBool(const void *handle, jstring oKey, jboolean value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((bool) value, key);
    }
    return false;
}

MMKV_EXPORT jboolean decodeBool(const void *handle, jstring oKey, jboolean defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getBool(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT jboolean encodeInt32(const void *handle, jstring oKey, jint value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((int32_t) value, key);
    }
    return false;
}

MMKV_EXPORT jint decodeInt32(const void *handle, jstring oKey, jint defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getInt32(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT jboolean encodeInt64(const void *handle, jstring oKey, jlong value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((int64_t) value, key);
    }
    return false;
}

MMKV_EXPORT jlong decodeInt64(const void *handle, jstring oKey, jlong defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getInt64(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT jboolean encodeDouble(const void *handle, jstring oKey, double value) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->set((double) value, key);
    }
    return false;
}

MMKV_EXPORT double decodeDouble(const void *handle, jstring oKey, double defaultValue) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        return kv->getDouble(key, defaultValue);
    }
    return defaultValue;
}

MMKV_EXPORT jboolean encodeBytes(const void *handle, jstring oKey, void *oValue, uint64_t length) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        if (oValue) {
            auto value = MMBuffer(oValue, static_cast<size_t>(length), MMBufferNoCopy);
            return kv->set(value, key);
        } else {
            kv->removeValueForKey(key);
            return true;
        }
    }
    return false;
}

MMKV_EXPORT void *decodeBytes(const void *handle, jstring oKey, uint64_t *lengthPtr) {
    MMKV *kv = reinterpret_cast<MMKV *>(handle);
    if (kv && oKey) {
        auto key = string(oKey);
        auto value = kv->getBytes(key);
        if (value.length() > 0) {
            if (value.isStoredOnStack()) {
                auto result = malloc(value.length());
                if (result) {
                    memcpy(result, value.getPtr(), value.length());
                    *lengthPtr = value.length();
                }
                return result;
            } else {
                void *result = value.getPtr();
                *lengthPtr = value.length();
                value.detach();
                return result;
            }
        }
    }
    return nullptr;
}

#endif // MMKV_POSIX
