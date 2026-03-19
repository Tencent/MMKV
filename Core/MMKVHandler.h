/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
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

#ifndef MMKVCoreHandler_h
#define MMKVCoreHandler_h

#include "MMKVPredef.h"

#ifdef __cplusplus

namespace mmkv {

// unified callback handler for MMKV
class MMKVHandler {
public:
    virtual ~MMKVHandler() = default;
    
    // by default MMKV will print log using system log
    // implement this method to redirect MMKV's log
    virtual void mmkvLog(MMKVLogLevel level, const char *file, int line, const char *function, MMKVLog_t message) {}
    
    // by default MMKV will discard all data on crc32-check failure
    // return `OnErrorRecover` to recover any data on the file
    virtual MMKVRecoverStrategic onMMKVCRCCheckFail(const std::string &mmapID) { return OnErrorDiscard; }
    
    // by default MMKV will discard all data on file length mismatch
    // return `OnErrorRecover` to recover any data on the file
    virtual MMKVRecoverStrategic onMMKVFileLengthError(const std::string &mmapID) { return OnErrorDiscard; }
    
    // called when content is changed by other process
    // doesn't guarantee real-time notification
    virtual void onContentChangedByOuterProcess(const std::string &mmapID) {}
    
    // called when an MMKV file is loaded successfully
    virtual void onMMKVContentLoadSuccessfully(const std::string &mmapID) {}
};

} // namespace mmkv

#endif

#endif /* MMKVHandler_h */
