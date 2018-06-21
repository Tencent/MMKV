//
//  MMKVLog.h
//  MMKV
//
//  Created by Ling Guo on 2018/6/20.
//  Copyright © 2018 Lingol. All rights reserved.
//

#ifndef MMKVLog_h
#define MMKVLog_h

#import <Foundation/Foundation.h>

// enable logging
#define ENABLE_MMKV_LOG

#ifdef ENABLE_MMKV_LOG

#define MMKVError(format, ...) NSLog(format, ##__VA_ARGS__)
#define MMKVWarning(format, ...) NSLog(format, ##__VA_ARGS__)
#define MMKVInfo(format, ...) NSLog(format, ##__VA_ARGS__)

#ifndef NDEBUG
#define MMKVDebug(format, ...) NSLog(format, ##__VA_ARGS__)
#else
#define MMKVDebug(format, ...) {}
#endif // NDEBUG

#else

#define MMKVError(format, ...) {}
#define MMKVWarning(format, ...) {}
#define MMKVInfo(format, ...) {}
#define MMKVDebug(format, ...) {}

#endif // ENABLE_MMKV_LOG

#endif /* MMKVLog_h */
