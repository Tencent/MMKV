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

#ifdef __ANDROID__
#   define MMKV_ANDROID
#elif __APPLE__
#   define MMKV_IOS_OR_MAC
#   ifdef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#       define MMKV_IOS
#   else
#       define MMKV_MAC
#   endif
#elif __linux__ || __unix__
#   define MMKV_POSIX
#elif _WIN32
#   define MMKV_WIN32
#endif

enum MMKVLogLevel : int {
    MMKVLogDebug = 0, // not available for release/product build
    MMKVLogInfo = 1,  // default level
    MMKVLogWarning,
    MMKVLogError,
    MMKVLogNone, // special level used to disable all log messages
};

#include <string>

namespace mmkv {

typedef void (*LogHandler)(MMKVLogLevel level,
                           const std::string &file,
                           int line,
                           const std::string &function,
                           const std::string &message);


}

#endif //MMKV_SRC_MMKVPREDEF_H
