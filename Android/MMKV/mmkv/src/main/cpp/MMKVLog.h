//
// Created by Ling Guo on 2018/4/28.
//

#ifndef MMKV_MMKVLOG_H
#define MMKV_MMKVLOG_H

#include <android/log.h>

#define APPNAME "MMKV"

#define MMKVError(format, ...)	    __android_log_print(ANDROID_LOG_ERROR, APPNAME, format, ##__VA_ARGS__)
#define MMKVWarning(format, ...)	__android_log_print(ANDROID_LOG_WARN, APPNAME, format, ##__VA_ARGS__)
#define MMKVInfo(format, ...)		__android_log_print(ANDROID_LOG_INFO, APPNAME, format, ##__VA_ARGS__)
#define MMKDebug(format, ...)	    __android_log_print(ANDROID_LOG_DEBUG, APPNAME, format, ##__VA_ARGS__)

#endif //MMKV_MMKVLOG_H
