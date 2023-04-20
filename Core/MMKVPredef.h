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

// disable encryption & decryption to reduce some code
//#define MMKV_DISABLE_CRYPT
//#define MMKV_DISABLE_FLUTTER

// using POSIX implementation
//#define FORCE_POSIX

#ifdef __cplusplus

#include <string>
#include <vector>
#include <unordered_map>

constexpr auto MMKV_VERSION = "v1.2.16";

#ifdef DEBUG
#    define MMKV_DEBUG
#endif

#ifdef NDEBUG
#    undef MMKV_DEBUG
#endif

#ifdef __ANDROID__
#    ifdef FORCE_POSIX
#        define MMKV_POSIX
#    else
#        define MMKV_ANDROID
#    endif
#elif __APPLE__
#    ifdef FORCE_POSIX
#        define MMKV_POSIX
#    else
#        define MMKV_APPLE
#        ifdef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#            define MMKV_IOS
#        elif __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__
#            define MMKV_WATCH
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
#    define MMKV_PATH_FORMAT "%ls"
using MMKVFileHandle_t = HANDLE;
using MMKVPath_t = std::wstring;
extern MMKVPath_t string2MMKVPath_t(const std::string &str);
extern std::string MMKVPath_t2String(const MMKVPath_t &str);

#    ifndef MMKV_EMBED_ZLIB
#        define MMKV_EMBED_ZLIB 1
#    endif

#else // MMKV_WIN32

constexpr auto MMKV_PATH_SLASH = "/";
#    define MMKV_PATH_FORMAT "%s"
using MMKVFileHandle_t = int;
using MMKVPath_t = std::string;
#    define string2MMKVPath_t(str) (str)
#    define MMKVPath_t2String(str) (str)

#    ifndef MMKV_EMBED_ZLIB
#        define MMKV_EMBED_ZLIB 0
#    endif

#endif // MMKV_WIN32

#ifdef MMKV_APPLE
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
#endif // MMKV_APPLE

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
struct KeyValueHolder;

#ifdef MMKV_DISABLE_CRYPT
using KeyValueHolderCrypt = KeyValueHolder;
#else
struct KeyValueHolderCrypt;
#endif

#ifdef MMKV_APPLE
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

using MMKVVector = std::vector<std::pair<NSString *, mmkv::MMBuffer>>;
using MMKVMap = std::unordered_map<NSString *, mmkv::KeyValueHolder, KeyHasher, KeyEqualer>;
using MMKVMapCrypt = std::unordered_map<NSString *, mmkv::KeyValueHolderCrypt, KeyHasher, KeyEqualer>;
#else
using MMKVVector = std::vector<std::pair<std::string, mmkv::MMBuffer>>;
using MMKVMap = std::unordered_map<std::string, mmkv::KeyValueHolder>;
using MMKVMapCrypt = std::unordered_map<std::string, mmkv::KeyValueHolderCrypt>;
#endif // MMKV_APPLE

template <typename T>
void unused(const T &) {}

constexpr size_t AES_KEY_LEN = 16;
constexpr size_t AES_KEY_BITSET_LEN = 128;

} // namespace mmkv

#ifdef MMKV_DEBUG
#    include <cassert>
#    define MMKV_ASSERT(var) assert(var)
#else
#    define MMKV_ASSERT(var) mmkv::unused(var)
#endif

#endif //cplus-plus

#if defined(__arm__)
  #if defined(__ARM_ARCH_7A__)
    #if defined(__ARM_NEON__)
      #if defined(__ARM_PCS_VFP)
        #define MMKV_ABI "armeabi-v7a/NEON (hard-float)"
      #else
        #define MMKV_ABI "armeabi-v7a/NEON"
      #endif
    #else
      #if defined(__ARM_PCS_VFP)
        #define MMKV_ABI "armeabi-v7a (hard-float)"
      #else
        #define MMKV_ABI "armeabi-v7a"
      #endif
    #endif
  #else
   #define MMKV_ABI "armeabi"
  #endif
#elif defined(__i386__) || defined(_M_IX86)
  #define MMKV_ABI "x86"
#elif defined(__x86_64__) || defined(_M_X64)
  #define MMKV_ABI "x86_64"
#elif defined(__mips64)
  #define MMKV_ABI "mips64"
#elif defined(__mips__)
  #define MMKV_ABI "mips"
#elif defined(__aarch64__) || defined(_M_ARM64)
  #define MMKV_ABI "arm64-v8a"
#else
  #define MMKV_ABI "unknown"
#endif

#endif //MMKV_SRC_MMKVPREDEF_H
