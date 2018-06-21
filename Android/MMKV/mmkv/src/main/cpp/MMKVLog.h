//
// Created by Ling Guo on 2018/4/28.
//

#ifndef MMKV_MMKVLOG_H
#define MMKV_MMKVLOG_H

#include <android/log.h>
#include <cstring>
#include <errno.h>

// enable logging
#define ENABLE_MMKV_LOG

#ifdef ENABLE_MMKV_LOG

#define APPNAME "MMKV"

#define MMKVError(format, ...)                                                                     \
    __android_log_print(ANDROID_LOG_ERROR, APPNAME, format, ##__VA_ARGS__)
#define MMKVWarning(format, ...)                                                                   \
    __android_log_print(ANDROID_LOG_WARN, APPNAME, format, ##__VA_ARGS__)
#define MMKVInfo(format, ...) __android_log_print(ANDROID_LOG_INFO, APPNAME, format, ##__VA_ARGS__)

#ifndef NDEBUG
#define MMKVDebug(format, ...)                                                                     \
    __android_log_print(ANDROID_LOG_DEBUG, APPNAME, format, ##__VA_ARGS__)
#else
#define MMKVDebug(format, ...)                                                                     \
    {}
#endif

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
