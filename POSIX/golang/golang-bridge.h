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

#ifdef __cplusplus
#    include <cstdint>
extern "C" {
#else
#    include <stdbool.h>
#    include <stdint.h>
#endif

typedef struct {
    const char *ptr;
    size_t length;
} GoStringWrap;

void mmkvInitialize(const char *rootDir, int32_t logLevel);
void onExit();
void *getMMKVWithID(const char *mmapID, int32_t mode, const char *cryptKey, const char *rootPath);
void *getDefaultMMKV(int32_t mode, const char *cryptKey);
const char *mmapID(void *handle);
bool encodeBool(void *handle, const char *oKey, bool value);
bool decodeBool(void *handle, const char *oKey, bool defaultValue);

bool encodeBool2(void *handle, GoStringWrap oKey, bool value);
bool decodeBool2(void *handle, GoStringWrap oKey, bool defaultValue);

bool encodeInt32(void *handle, const char *oKey, int32_t value);
int32_t decodeInt32(void *handle, const char *oKey, int32_t defaultValue);
bool encodeUInt32(void *handle, const char *oKey, uint32_t value);
uint32_t decodeUInt32(void *handle, const char *oKey, uint32_t defaultValue);
bool encodeInt64(void *handle, const char *oKey, int64_t value);
int64_t decodeInt64(void *handle, const char *oKey, int64_t defaultValue);
bool encodeUInt64(void *handle, const char *oKey, uint64_t value);
uint64_t decodeUInt64(void *handle, const char *oKey, uint64_t defaultValue);
bool encodeFloat(void *handle, const char *oKey, float value);
float decodeFloat(void *handle, const char *oKey, float defaultValue);
bool encodeDouble(void *handle, const char *oKey, double value);
double decodeDouble(void *handle, const char *oKey, double defaultValue);
bool encodeBytes(void *handle, const char *oKey, void *oValue, uint64_t length);
void *decodeBytes(void *handle, const char *oKey, uint64_t *lengthPtr);
bool reKey(void *handle, char *oKey, uint32_t length);
void *cryptKey(void *handle, uint32_t *lengthPtr);
void checkReSetCryptKey(void *handle, char *oKey, uint64_t length);
uint64_t allKeys(void *handle, char ***keyArrayPtr, uint32_t **sizeArrayPtr);
bool containsKey(void *handle, char *oKey);
uint64_t count(void *handle);
uint64_t totalSize(void *handle);
uint64_t actualSize(void *handle);
void removeValueForKey(void *handle, char *oKey);
void removeValuesForKeys(void *handle, char **keyArray, uint32_t *sizeArray, uint64_t count);
void clearAll(void *handle);
void mmkvSync(void *handle, bool sync);
void clearMemoryCache(void *handle);
int32_t pageSize();
const char *version();
void trim(void *handle);
void mmkvClose(void *handle);

#ifdef __cplusplus
}
#endif
