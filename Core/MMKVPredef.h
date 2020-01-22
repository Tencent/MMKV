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
#include <unordered_map>

#ifdef __ANDROID__
#    define MMKV_ANDROID
#elif __APPLE__
#    ifdef FORCE_POSIX
#        define MMKV_POSIX
#    else
#        define MMKV_IOS_OR_MAC
#        ifdef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#            define MMKV_IOS
#        else
#            define MMKV_MAC
#        endif
#    endif // FORCE_POSIX
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
// Exclude rarely-used stuff from Windows headers
#    define WIN32_LEAN_AND_MEAN
// Windows Header Files
#    include <windows.h>

constexpr auto MMKV_PATH_SLASH = L"\\";
#    define MMKV_PATH_FORMAT "%ws"
using MMKVFileHandle_t = HANDLE;
using MMKVPath_t = std::wstring;
extern MMKVPath_t string2MMKVPath_t(const std::string &str);

#    ifndef MMKV_EMBED_ZLIB
#        define MMKV_EMBED_ZLIB 1
#    endif

#else // MMKV_WIN32

constexpr auto MMKV_PATH_SLASH = "/";
#    define MMKV_PATH_FORMAT "%s"
using MMKVFileHandle_t = int;
using MMKVPath_t = std::string;
#    define string2MMKVPath_t(str) (str)

#    ifndef MMKV_EMBED_ZLIB
#        define MMKV_EMBED_ZLIB 0
#    endif

#endif // MMKV_WIN32

#ifdef MMKV_IOS_OR_MAC
#    import <Foundation/Foundation.h>
#    define MMKV_NAMESPACE_BEGIN namespace mmkv {
#    define MMKV_NAMESPACE_END }
#    define MMKV_NAMESPACE_PREFIX mmkv
using MMKVLog_t = NSString *;
#else
#    define MMKV_NAMESPACE_BEGIN
#    define MMKV_NAMESPACE_END
#    define MMKV_NAMESPACE_PREFIX
using MMKVLog_t = const std::string &;
#endif // MMKV_IOS_OR_MAC

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

enum SyncFlag : bool { MMKV_SYNC = true, MMKV_ASYNC = false };

MMKV_NAMESPACE_END

namespace mmkv {

typedef void (*LogHandler)(MMKVLogLevel level, const char *file, int line, const char *function, MMKVLog_t message);

// by default MMKV will discard all datas on failure
// return `OnErrorRecover` to recover any data from file
typedef MMKVRecoverStrategic (*ErrorHandler)(const std::string &mmapID, MMKVErrorType errorType);

// called when content is changed by other process
// doesn't guarantee real-time notification
typedef void (*ContentChangeHandler)(const std::string &mmapID);

extern size_t DEFAULT_MMAP_SIZE;
#define DEFAULT_MMAP_ID "mmkv.default"

class MMBuffer;

#ifdef MMKV_IOS_OR_MAC
struct KeyHasher {
    size_t operator()(NSString *key) const { return key.hash; }
};

struct KeyEqualer {
    bool operator()(NSString *left, NSString *right) const {
        if (left == right) {
            return true;
        }
        return ([left isEqualToString:right] == YES);
    }
};

using MMKVMap = std::unordered_map<NSString *, mmkv::MMBuffer, KeyHasher, KeyEqualer>;
#else
using MMKVMap = std::unordered_map<std::string, mmkv::MMBuffer>;
#endif // MMKV_IOS_OR_MAC

template <typename T>
void unused(const T &) {}

} // namespace mmkv

#ifndef NDEBUG
#    include <cassert>
#    define MMKV_ASSERT(var) assert(var)
#else
#    define MMKV_ASSERT(var) mmkv::unused(var)
#endif

#endif //MMKV_SRC_MMKVPREDEF_H
