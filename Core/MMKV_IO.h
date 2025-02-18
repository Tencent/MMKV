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

#ifndef MMKV_IO_h
#define MMKV_IO_h
#ifdef __cplusplus

#include "MMKV.h"

MMKV_NAMESPACE_BEGIN

std::string mmapedKVKey(const std::string &mmapID, const MMKVPath_t *rootPath = nullptr);
std::string legacyMmapedKVKey(const std::string &mmapID, const MMKVPath_t *rootPath = nullptr);
#ifndef MMKV_ANDROID
MMKVPath_t mappedKVPathWithID(const std::string &mmapID, const MMKVPath_t *rootPath);
#else
MMKVPath_t mappedKVPathWithID(const std::string &mmapID, const MMKVPath_t *rootPath, MMKVMode mode = MMKV_MULTI_PROCESS);
#endif
MMKVPath_t crcPathWithPath(const MMKVPath_t &kvPath);

MMKVRecoverStrategic onMMKVCRCCheckFail(const std::string &mmapID);
MMKVRecoverStrategic onMMKVFileLengthError(const std::string &mmapID);

template <typename T>
void clearDictionary(T *dic) {
    if (!dic) {
        return;
    }
#ifdef MMKV_APPLE
    for (auto &pair : *dic) {
        [pair.first release];
    }
#endif
    dic->clear();
}

enum : bool {
    KeepSequence = false,
    IncreaseSequence = true,
};

#ifdef MMKV_ANDROID
// status of migrating old file to new file
enum class MigrateStatus: uint32_t {
    NotSpecial, // it's not specially (mistakenly) encoded
    NoneExist, // none of these files exist
    NewExist, // only new file exist
    OldToNewMigrated, // migrated, it's one time only operation
    OldToNewMigrateFail, // old file exist but fail to migrate (maybe other process opened)
    OldAndNewExist, // both old and new exist (fail to delete old file? old file migrated from old device?)
};

// historically Android mistakenly use mmapKey as mmapID, we try migrate back to normal when possible
MigrateStatus tryMigrateLegacyMMKVFile(const std::string &mmapID, const MMKVPath_t *rootPath);
#endif // MMKV_ANDROID

MMKV_NAMESPACE_END

#endif
#endif /* MMKV_IO_h */
