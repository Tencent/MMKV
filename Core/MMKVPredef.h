/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
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

#ifndef MMKV_SRC_MMKVPREDEF_H
#define MMKV_SRC_MMKVPREDEF_H

#include <string>

#ifdef __ANDROID__
#    define MMKV_ANDROID
#elif __APPLE__
#    define MMKV_IOS_OR_MAC
#    ifdef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#        define MMKV_IOS
#    else
#        define MMKV_MAC
#    endif
#elif __linux__ || __unix__
#    define MMKV_POSIX
#    if __linux__
#        define MMKV_LINUX
#    endif
#elif _WIN32
#    define MMKV_WIN32
#endif

#ifdef MMKV_WIN32
#    if !defined(_WIN32_WINNT)
#        define _WIN32_WINNT _WIN32_WINNT_WINXP
#    endif

#    include <SDKDDKVer.h>

#    define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers                  \
                                // Windows Header Files
#    include <windows.h>

constexpr auto MMKV_PATH_SLASH = L"\\";
#    define MMKV_PATH_FORMAT "%ws"
using MMKV_FILE_HANDLE = HANDLE;
using MMKV_PATH_TYPE = std::wstring;
extern MMKV_PATH_TYPE string2MMKV_PATH_TYPE(const std::string &str);
#else
constexpr auto MMKV_PATH_SLASH = "/";
#    define MMKV_PATH_FORMAT "%s"
using MMKV_FILE_HANDLE = int;
using MMKV_PATH_TYPE = std::string;
#    define string2MMKV_PATH_TYPE(str) (str)
#endif // MMKV_WIN32

#ifdef MMKV_IOS_OR_MAC
#    import <Foundation/Foundation.h>
#    define MMKV_NAMESPACE_BEGIN namespace mmkv {
#    define MMKV_NAMESPACE_END }
#else
#    define MMKV_NAMESPACE_BEGIN
#    define MMKV_NAMESPACE_END
#endif

MMKV_NAMESPACE_BEGIN

enum MMKVLogLevel : int {
    MMKVLogDebug = 0, // not available for release/product build
    MMKVLogInfo = 1,  // default level
    MMKVLogWarning,
    MMKVLogError,
    MMKVLogNone, // special level used to disable all log messages
};

enum MMKVRecoverStrategic : int {
    OnErrorDiscard = 0,
    OnErrorRecover,
};

enum MMKVErrorType : int {
    MMKVCRCCheckFail = 0,
    MMKVFileLength,
};

MMKV_NAMESPACE_END

namespace mmkv {

typedef void (*LogHandler)(MMKVLogLevel level,
                           const std::string &file,
                           int line,
                           const std::string &function,
                           const std::string &message);

typedef MMKVRecoverStrategic (*ErrorHandler)(const std::string &mmapID, MMKVErrorType errorType);

typedef void (*ContentChangeHandler)(const std::string &mmapID);

extern size_t DEFAULT_MMAP_SIZE;

} // namespace mmkv

#endif //MMKV_SRC_MMKVPREDEF_H
