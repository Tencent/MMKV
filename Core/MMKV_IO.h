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

std::string mmapedKVKey(const std::string &mmapID, MMKVPath_t *rootPath = nullptr);
MMKVPath_t mappedKVPathWithID(const std::string &mmapID, MMKVMode mode, MMKVPath_t *rootPath);
MMKVPath_t crcPathWithID(const std::string &mmapID, MMKVMode mode, MMKVPath_t *rootPath);

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

MMKV_NAMESPACE_END

#endif
#endif /* MMKV_IO_h */
