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

#include "MMKV.h"
#include <cstdio>
#include <cstring>

// enable logging
#define ENABLE_MMKV_LOG

#ifdef ENABLE_MMKV_LOG

extern MMKVLogLevel g_currentLogLevel;
extern MMKV::LogHandler g_logHandler;

#define __filename__ (strrchr(__FILE__, '\\') + 1)

#define MMKVError(format, ...)                                                                     \
    _MMKVLogWithLevel(MMKVLogError, __filename__, __func__, __LINE__, format, ##__VA_ARGS__)
#define MMKVWarning(format, ...)                                                                   \
    _MMKVLogWithLevel(MMKVLogWarning, __filename__, __func__, __LINE__, format, ##__VA_ARGS__)
#define MMKVInfo(format, ...)                                                                      \
    _MMKVLogWithLevel(MMKVLogInfo, __filename__, __func__, __LINE__, format, ##__VA_ARGS__)

#ifndef NDEBUG
#define MMKVDebug(format, ...)                                                                     \
    _MMKVLogWithLevel(MMKVLogDebug, __filename__, __func__, __LINE__, format, ##__VA_ARGS__)
#else
#define MMKVDebug(format, ...)                                                                     \
    {}
#endif // NDEBUG

void _MMKVLogWithLevel(
    MMKVLogLevel level, const char *file, const char *func, int line, const char *format, ...);

#else

#define MMKVError(format, ...)                                                                     \
    {}
#define MMKVWarning(format, ...)                                                                   \
    {}
#define MMKVInfo(format, ...)                                                                      \
    {}
#define MMKVDebug(format, ...)                                                                     \
    {}

#endif

#endif //MMKV_MMKVLOG_H
