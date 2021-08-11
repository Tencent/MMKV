/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
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

#ifndef MMKV_MMKVLOG_H
#define MMKV_MMKVLOG_H
#ifdef __cplusplus

#include "MMKVPredef.h"

#include <cerrno>
#include <cstdint>
#include <cstring>

void _MMKVLogWithLevel(
    MMKV_NAMESPACE_PREFIX::MMKVLogLevel level, const char *filename, const char *func, int line, const char *format, ...);

MMKV_NAMESPACE_BEGIN

extern MMKVLogLevel g_currentLogLevel;
extern mmkv::LogHandler g_logHandler;

// enable logging
#define ENABLE_MMKV_LOG

#ifdef ENABLE_MMKV_LOG

#    ifdef __FILE_NAME__
#        define __MMKV_FILE_NAME__ __FILE_NAME__
#    else
const char *_getFileName(const char *path);
#        define __MMKV_FILE_NAME__ MMKV_NAMESPACE_PREFIX::_getFileName(__FILE__)
#    endif

#    define MMKVError(format, ...)                                                                                     \
        _MMKVLogWithLevel(MMKV_NAMESPACE_PREFIX::MMKVLogError, __MMKV_FILE_NAME__, __func__, __LINE__, format,         \
                          ##__VA_ARGS__)
#    define MMKVWarning(format, ...)                                                                                   \
        _MMKVLogWithLevel(MMKV_NAMESPACE_PREFIX::MMKVLogWarning, __MMKV_FILE_NAME__, __func__, __LINE__, format,       \
                          ##__VA_ARGS__)
#    define MMKVInfo(format, ...)                                                                                      \
        _MMKVLogWithLevel(MMKV_NAMESPACE_PREFIX::MMKVLogInfo, __MMKV_FILE_NAME__, __func__, __LINE__, format,          \
                          ##__VA_ARGS__)

#    ifdef MMKV_DEBUG
#        define MMKVDebug(format, ...)                                                                                 \
            _MMKVLogWithLevel(MMKV_NAMESPACE_PREFIX::MMKVLogDebug, __MMKV_FILE_NAME__, __func__, __LINE__, format,     \
                              ##__VA_ARGS__)
#    else
#        define MMKVDebug(format, ...)                                                                                 \
            {}
#    endif

#else

#    define MMKVError(format, ...)                                                                                     \
        {}
#    define MMKVWarning(format, ...)                                                                                   \
        {}
#    define MMKVInfo(format, ...)                                                                                      \
        {}
#    define MMKVDebug(format, ...)                                                                                     \
        {}

#endif

MMKV_NAMESPACE_END

#endif
#endif //MMKV_MMKVLOG_H
