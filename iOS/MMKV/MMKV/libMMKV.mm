/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

#import "MMKV.h"
#import <MMKVCore/MMKV.h>
#import <MMKVCore/MMKVLog.h>
#import <MMKVCore/ScopedLock.hpp>
#import <MMKVCore/ThreadLock.h>
#import <MMKVCore/openssl_md5.h>

#if defined(MMKV_IOS) && !defined(MMKV_IOS_EXTENSION)
#import <UIKit/UIKit.h>
#endif

#if __has_feature(objc_arc)
#error This file must be compiled with MRC. Use -fno-objc-arc flag.
#endif

using namespace std;

static NSMutableDictionary *g_instanceDic = nil;
static mmkv::ThreadLock *g_lock;
static id<MMKVHandler> g_callbackHandler = nil;
static bool g_isLogRedirecting = false;
static NSString *g_basePath = nil;
static NSString *g_groupPath = nil;

#if defined(MMKV_IOS) && !defined(MMKV_IOS_EXTENSION)
static BOOL g_isRunningInAppExtension = NO;
#endif

static void LogHandler(mmkv::MMKVLogLevel level, const char *file, int line, const char *function, NSString *message);
static mmkv::MMKVRecoverStrategic ErrorHandler(const string &mmapID, mmkv::MMKVErrorType errorType);
static void ContentChangeHandler(const string &mmapID);

@implementation MMKV {
    NSString *m_mmapID;
    NSString *m_mmapKey;
    mmkv::MMKV *m_mmkv;
    uint64_t m_lastAccessTime;
}

#pragma mark - init

+ (NSString *)initializeMMKV:(nullable NSString *)rootDir {
    return [MMKV initializeMMKV:rootDir logLevel:MMKVLogInfo handler:nil];
}

+ (NSString *)initializeMMKV:(nullable NSString *)rootDir logLevel:(MMKVLogLevel)logLevel {
    return [MMKV initializeMMKV:rootDir logLevel:logLevel handler:nil];
}

static BOOL g_hasCalledInitializeMMKV = NO;

+ (NSString *)initializeMMKV:(nullable NSString *)rootDir logLevel:(MMKVLogLevel)logLevel handler:(id<MMKVHandler>)handler {
    if (g_hasCalledInitializeMMKV) {
        MMKVWarning("already called +initializeMMKV before, ignore this request");
        return [self mmkvBasePath];
    }
    g_instanceDic = [[NSMutableDictionary alloc] init];
    g_lock = new mmkv::ThreadLock();
    g_lock->initialize();

    g_callbackHandler = handler;
    mmkv::LogHandler logHandler = nullptr;
    if (g_callbackHandler && [g_callbackHandler respondsToSelector:@selector(mmkvLogWithLevel:file:line:func:message:)]) {
        g_isLogRedirecting = true;
        logHandler = LogHandler;
    }

    if (rootDir != nil) {
        [g_basePath release];
        g_basePath = [rootDir retain];
    } else {
        [self mmkvBasePath];
    }
    NSAssert(g_basePath.length > 0, @"MMKV not initialized properly, must not call +initializeMMKV: before -application:didFinishLaunchingWithOptions:");
    mmkv::MMKV::initializeMMKV(g_basePath.UTF8String, (mmkv::MMKVLogLevel) logLevel, logHandler);

    if ([g_callbackHandler respondsToSelector:@selector(onMMKVCRCCheckFail:)] ||
        [g_callbackHandler respondsToSelector:@selector(onMMKVFileLengthError:)]) {
        mmkv::MMKV::registerErrorHandler(ErrorHandler);
    }
    if ([g_callbackHandler respondsToSelector:@selector(onMMKVContentChange:)]) {
        mmkv::MMKV::registerContentChangeHandler(ContentChangeHandler);
    }

#if defined(MMKV_IOS) && !defined(MMKV_IOS_EXTENSION)
    // just in case someone forget to set the MMKV_IOS_EXTENSION macro
    if ([[[NSBundle mainBundle] bundlePath] hasSuffix:@".appex"]) {
        g_isRunningInAppExtension = YES;
    }
    if (!g_isRunningInAppExtension) {
        auto appState = [UIApplication sharedApplication].applicationState;
        auto isInBackground = (appState == UIApplicationStateBackground);
        mmkv::MMKV::setIsInBackground(isInBackground);
        MMKVInfo("appState:%ld", (long) appState);

        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didEnterBackground) name:UIApplicationDidEnterBackgroundNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didBecomeActive) name:UIApplicationDidBecomeActiveNotification object:nil];
    }
#endif

    g_hasCalledInitializeMMKV = YES;

    return [self mmkvBasePath];
}

+ (NSString *)initializeMMKV:(nullable NSString *)rootDir groupDir:(NSString *)groupDir logLevel:(MMKVLogLevel)logLevel {
    auto ret = [MMKV initializeMMKV:rootDir logLevel:logLevel handler:nil];

    g_groupPath = [[groupDir stringByAppendingPathComponent:@"mmkv"] retain];
    MMKVInfo("groupDir: %@", g_groupPath);

    return ret;
}

+ (NSString *)initializeMMKV:(nullable NSString *)rootDir groupDir:(NSString *)groupDir logLevel:(MMKVLogLevel)logLevel handler:(nullable id<MMKVHandler>)handler {
    auto ret = [MMKV initializeMMKV:rootDir logLevel:logLevel handler:handler];

    g_groupPath = [[groupDir stringByAppendingPathComponent:@"mmkv"] retain];
    MMKVInfo("groupDir: %@", g_groupPath);

    return ret;
}

// a generic purpose instance
+ (instancetype)defaultMMKV {
    return [MMKV mmkvWithID:(@"" DEFAULT_MMAP_ID) cryptKey:nil rootPath:nil mode:MMKVSingleProcess];
}

+ (nullable instancetype)defaultMMKVWithCryptKey:(nullable NSData *)cryptKey {
    return [MMKV mmkvWithID:(@"" DEFAULT_MMAP_ID) cryptKey:cryptKey rootPath:nil mode:MMKVSingleProcess];
}

// any unique ID (com.tencent.xin.pay, etc)
+ (instancetype)mmkvWithID:(NSString *)mmapID {
    return [MMKV mmkvWithID:mmapID cryptKey:nil rootPath:nil mode:MMKVSingleProcess];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID mode:(MMKVMode)mode {
    auto rootPath = (mode == MMKVSingleProcess) ? nil : g_groupPath;
    return [MMKV mmkvWithID:mmapID cryptKey:nil rootPath:rootPath mode:mode];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey {
    return [MMKV mmkvWithID:mmapID cryptKey:cryptKey rootPath:nil mode:MMKVSingleProcess];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey mode:(MMKVMode)mode {
    auto rootPath = (mode == MMKVSingleProcess) ? nil : g_groupPath;
    return [MMKV mmkvWithID:mmapID cryptKey:cryptKey rootPath:rootPath mode:mode];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID rootPath:(nullable NSString *)rootPath {
    return [MMKV mmkvWithID:mmapID cryptKey:nil rootPath:rootPath mode:MMKVSingleProcess];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID relativePath:(nullable NSString *)relativePath {
    return [MMKV mmkvWithID:mmapID cryptKey:nil rootPath:relativePath mode:MMKVSingleProcess];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey rootPath:(nullable NSString *)rootPath {
    return [MMKV mmkvWithID:mmapID cryptKey:cryptKey rootPath:rootPath mode:MMKVSingleProcess];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey relativePath:(nullable NSString *)relativePath {
    return [MMKV mmkvWithID:mmapID cryptKey:cryptKey rootPath:relativePath mode:MMKVSingleProcess];
}

// relatePath and MMKVMultiProcess mode can't be set at the same time, so we hide this method from public
+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey rootPath:(nullable NSString *)rootPath mode:(MMKVMode)mode {
    NSAssert(g_hasCalledInitializeMMKV, @"MMKV not initialized properly, must call +initializeMMKV: in main thread before calling any other MMKV methods");
    if (mmapID.length <= 0) {
        return nil;
    }

    SCOPED_LOCK(g_lock);

    if (mode == MMKVMultiProcess) {
        if (!rootPath) {
            rootPath = g_groupPath;
        }
        if (!rootPath) {
            MMKVError("Getting a multi-process MMKV [%@] without setting groupDir makes no sense", mmapID);
            MMKV_ASSERT(0);
        }
    }
    NSString *kvKey = [MMKV mmapKeyWithMMapID:mmapID rootPath:rootPath];
    MMKV *kv = [g_instanceDic objectForKey:kvKey];
    if (kv == nil) {
        kv = [[MMKV alloc] initWithMMapID:mmapID cryptKey:cryptKey rootPath:rootPath mode:mode];
        if (!kv->m_mmkv) {
            [kv release];
            return nil;
        }
        kv->m_mmapKey = kvKey;
        [g_instanceDic setObject:kv forKey:kvKey];
        [kv release];
    }
    kv->m_lastAccessTime = [NSDate timeIntervalSinceReferenceDate];
    return kv;
}

- (instancetype)initWithMMapID:(NSString *)mmapID cryptKey:(NSData *)cryptKey rootPath:(NSString *)rootPath mode:(MMKVMode)mode {
    if (self = [super init]) {
        string pathTmp;
        if (rootPath.length > 0) {
            pathTmp = rootPath.UTF8String;
        }
        string cryptKeyTmp;
        if (cryptKey.length > 0) {
            cryptKeyTmp = string((char *) cryptKey.bytes, cryptKey.length);
        }
        string *rootPathPtr = pathTmp.empty() ? nullptr : &pathTmp;
        string *cryptKeyPtr = cryptKeyTmp.empty() ? nullptr : &cryptKeyTmp;
        m_mmkv = mmkv::MMKV::mmkvWithID(mmapID.UTF8String, (mmkv::MMKVMode) mode, cryptKeyPtr, rootPathPtr);
        if (!m_mmkv) {
            return self;
        }
        m_mmapID = [[NSString alloc] initWithUTF8String:m_mmkv->mmapID().c_str()];

#if defined(MMKV_IOS) && !defined(MMKV_IOS_EXTENSION)
        if (!g_isRunningInAppExtension) {
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(onMemoryWarning)
                                                         name:UIApplicationDidReceiveMemoryWarningNotification
                                                       object:nil];
        }
#endif
    }
    return self;
}

- (void)dealloc {
    [self clearMemoryCache];

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    MMKVInfo("dealloc %@", m_mmapID);
    [m_mmapID release];

    if (m_mmkv) {
        m_mmkv->close();
        m_mmkv = nullptr;
    }

    [super dealloc];
}

- (NSString *)mmapID {
    return m_mmapID;
}

#pragma mark - Application state

#if defined(MMKV_IOS) && !defined(MMKV_IOS_EXTENSION)
- (void)onMemoryWarning {
    MMKVInfo("cleaning on memory warning %@", m_mmapID);

    [self clearMemoryCache];
}

+ (void)didEnterBackground {
    mmkv::MMKV::setIsInBackground(true);
    MMKVInfo("isInBackground:%d", true);
}

+ (void)didBecomeActive {
    mmkv::MMKV::setIsInBackground(false);
    MMKVInfo("isInBackground:%d", false);
}
#endif

- (void)clearAll {
    m_mmkv->clearAll();
}

- (void)clearMemoryCache {
    if (m_mmkv) {
        m_mmkv->clearMemoryCache();
    }
}

- (void)close {
    SCOPED_LOCK(g_lock);
    MMKVInfo("closing %@", m_mmapID);

    [g_instanceDic removeObjectForKey:m_mmapKey];
}

- (void)trim {
    m_mmkv->trim();
}

#pragma mark - encryption & decryption

#ifndef MMKV_DISABLE_CRYPT

- (nullable NSData *)cryptKey {
    auto str = m_mmkv->cryptKey();
    if (str.length() > 0) {
        return [NSData dataWithBytes:str.data() length:str.length()];
    }
    return nil;
}

- (BOOL)reKey:(nullable NSData *)newKey {
    string key;
    if (newKey.length > 0) {
        key = string((char *) newKey.bytes, newKey.length);
    }
    return m_mmkv->reKey(key);
}

- (void)checkReSetCryptKey:(nullable NSData *)cryptKey {
    if (cryptKey.length > 0) {
        string key = string((char *) cryptKey.bytes, cryptKey.length);
        m_mmkv->checkReSetCryptKey(&key);
    } else {
        m_mmkv->checkReSetCryptKey(nullptr);
    }
}

#else

- (nullable NSData *)cryptKey {
    return nil;
}

- (BOOL)reKey:(nullable NSData *)newKey {
    return NO;
}

- (void)checkReSetCryptKey:(nullable NSData *)cryptKey {
}

#endif // MMKV_DISABLE_CRYPT

#pragma mark - set & get

- (BOOL)setObject:(nullable NSObject<NSCoding> *)object forKey:(NSString *)key {
    return m_mmkv->set(object, key);
}

- (BOOL)setObject:(nullable NSObject<NSCoding> *)object forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set(object, key, seconds);
}

- (BOOL)setBool:(BOOL)value forKey:(NSString *)key {
    return m_mmkv->set((bool) value, key);
}

- (BOOL)setBool:(BOOL)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set((bool) value, key, seconds);
}

- (BOOL)setInt32:(int32_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setInt32:(int32_t)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set(value, key, seconds);
}

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set(value, key, seconds);
}

- (BOOL)setInt64:(int64_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setInt64:(int64_t)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set(value, key, seconds);
}

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set(value, key, seconds);
}

- (BOOL)setFloat:(float)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setFloat:(float)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set(value, key, seconds);
}

- (BOOL)setDouble:(double)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setDouble:(double)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return m_mmkv->set(value, key, seconds);
}

- (BOOL)setString:(NSString *)value forKey:(NSString *)key {
    return [self setObject:value forKey:key];
}

- (BOOL)setString:(NSString *)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return [self setObject:value forKey:key expireDuration:seconds];
}

- (BOOL)setDate:(NSDate *)value forKey:(NSString *)key {
    return [self setObject:value forKey:key];
}

- (BOOL)setDate:(NSDate *)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return [self setObject:value forKey:key expireDuration:seconds];
}

- (BOOL)setData:(NSData *)value forKey:(NSString *)key {
    return [self setObject:value forKey:key];
}

- (BOOL)setData:(NSData *)value forKey:(NSString *)key expireDuration:(uint32_t)seconds {
    return [self setObject:value forKey:key expireDuration:seconds];
}

- (id)getObjectOfClass:(Class)cls forKey:(NSString *)key {
    return m_mmkv->getObject(key, cls);
}

- (BOOL)getBoolForKey:(NSString *)key {
    return [self getBoolForKey:key defaultValue:FALSE hasValue:nil];
}
- (BOOL)getBoolForKey:(NSString *)key defaultValue:(BOOL)defaultValue {
    return [self getBoolForKey:key defaultValue:defaultValue hasValue:nil];
}
- (BOOL)getBoolForKey:(NSString *)key defaultValue:(BOOL)defaultValue hasValue:(OUT BOOL *)hasValue {
    return m_mmkv->getBool(key, defaultValue, (bool *) hasValue);
}

- (int32_t)getInt32ForKey:(NSString *)key {
    return [self getInt32ForKey:key defaultValue:0 hasValue:nil];
}
- (int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue {
    return [self getInt32ForKey:key defaultValue:defaultValue hasValue:nil];
}
- (int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue hasValue:(OUT BOOL *)hasValue {
    return m_mmkv->getInt32(key, defaultValue, (bool *) hasValue);
}

- (uint32_t)getUInt32ForKey:(NSString *)key {
    return [self getUInt32ForKey:key defaultValue:0 hasValue:nil];
}
- (uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue {
    return [self getUInt32ForKey:key defaultValue:defaultValue hasValue:nil];
}
- (uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue hasValue:(OUT BOOL *)hasValue {
    return m_mmkv->getUInt32(key, defaultValue, (bool *) hasValue);
}

- (int64_t)getInt64ForKey:(NSString *)key {
    return [self getInt64ForKey:key defaultValue:0 hasValue:nil];
}
- (int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue {
    return [self getInt64ForKey:key defaultValue:defaultValue hasValue:nil];
}
- (int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue hasValue:(OUT BOOL *)hasValue {
    return m_mmkv->getInt64(key, defaultValue, (bool *) hasValue);
}

- (uint64_t)getUInt64ForKey:(NSString *)key {
    return [self getUInt64ForKey:key defaultValue:0 hasValue:nil];
}
- (uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue {
    return [self getUInt64ForKey:key defaultValue:defaultValue hasValue:nil];
}
- (uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue hasValue:(OUT BOOL *)hasValue {
    return m_mmkv->getUInt64(key, defaultValue, (bool *) hasValue);
}

- (float)getFloatForKey:(NSString *)key {
    return [self getFloatForKey:key defaultValue:0 hasValue:nil];
}
- (float)getFloatForKey:(NSString *)key defaultValue:(float)defaultValue {
    return [self getFloatForKey:key defaultValue:defaultValue hasValue:nil];
}
- (float)getFloatForKey:(NSString *)key defaultValue:(float)defaultValue hasValue:(OUT BOOL *)hasValue {
    return m_mmkv->getFloat(key, defaultValue, (bool *) hasValue);
}

- (double)getDoubleForKey:(NSString *)key {
    return [self getDoubleForKey:key defaultValue:0 hasValue:nil];
}
- (double)getDoubleForKey:(NSString *)key defaultValue:(double)defaultValue {
    return [self getDoubleForKey:key defaultValue:defaultValue hasValue:nil];
}
- (double)getDoubleForKey:(NSString *)key defaultValue:(double)defaultValue hasValue:(OUT BOOL *)hasValue {
    return m_mmkv->getDouble(key, defaultValue, (bool *) hasValue);
}

- (nullable NSString *)getStringForKey:(NSString *)key {
    return [self getStringForKey:key defaultValue:nil hasValue:nil];
}
- (nullable NSString *)getStringForKey:(NSString *)key defaultValue:(nullable NSString *)defaultValue {
    return [self getStringForKey:key defaultValue:defaultValue hasValue:nil];
}
- (nullable NSString *)getStringForKey:(NSString *)key defaultValue:(nullable NSString *)defaultValue hasValue:(OUT BOOL *)hasValue {
    if (key.length <= 0) {
        return defaultValue;
    }
    NSString *valueString = [self getObjectOfClass:NSString.class forKey:key];
    if (!valueString) {
        if (hasValue != nil) {
            *hasValue = false;
        }
        valueString = defaultValue;
    }
    return valueString;
}

- (nullable NSDate *)getDateForKey:(NSString *)key {
    return [self getDateForKey:key defaultValue:nil hasValue:nil];
}
- (nullable NSDate *)getDateForKey:(NSString *)key defaultValue:(nullable NSDate *)defaultValue {
    return [self getDateForKey:key defaultValue:defaultValue hasValue:nil];
}
- (nullable NSDate *)getDateForKey:(NSString *)key defaultValue:(nullable NSDate *)defaultValue hasValue:(OUT BOOL *)hasValue {
    if (key.length <= 0) {
        return defaultValue;
    }
    NSDate *valueDate = [self getObjectOfClass:NSDate.class forKey:key];
    if (!valueDate) {
        if (hasValue != nil) {
            *hasValue = false;
        }
        valueDate = defaultValue;
    }
    return valueDate;
}

- (nullable NSData *)getDataForKey:(NSString *)key {
    return [self getDataForKey:key defaultValue:nil];
}
- (nullable NSData *)getDataForKey:(NSString *)key defaultValue:(nullable NSData *)defaultValue {
    return [self getDataForKey:key defaultValue:defaultValue hasValue:nil];
}
- (nullable NSData *)getDataForKey:(NSString *)key defaultValue:(nullable NSData *)defaultValue hasValue:(OUT BOOL *)hasValue {
    if (key.length <= 0) {
        return defaultValue;
    }
    NSData *valueData = [self getObjectOfClass:NSData.class forKey:key];
    if (!valueData) {
        if (hasValue != nil) {
            *hasValue = false;
        }
        valueData = defaultValue;
    }
    return valueData;
}

- (size_t)getValueSizeForKey:(NSString *)key actualSize:(BOOL)actualSize {
    return m_mmkv->getValueSize(key, actualSize);
}

- (int32_t)writeValueForKey:(NSString *)key toBuffer:(NSMutableData *)buffer {
    return m_mmkv->writeValueToBuffer(key, buffer.mutableBytes, static_cast<int32_t>(buffer.length));
}

#pragma mark - enumerate

- (BOOL)containsKey:(NSString *)key {
    return m_mmkv->containsKey(key);
}

- (size_t)count {
    return m_mmkv->count();
}

- (size_t)totalSize {
    return m_mmkv->totalSize();
}

- (size_t)actualSize {
    return m_mmkv->actualSize();
}

+ (size_t)pageSize {
    return mmkv::DEFAULT_MMAP_SIZE;
}

+ (NSString *)version {
    return [NSString stringWithCString:MMKV_VERSION encoding:NSASCIIStringEncoding];
}

- (void)enumerateKeys:(void (^)(NSString *key, BOOL *stop))block {
    m_mmkv->enumerateKeys(block);
}

- (NSArray *)allKeys {
    return m_mmkv->allKeys();
}

- (BOOL)enableAutoKeyExpire:(uint32_t) expiredInSeconds {
    return m_mmkv->enableAutoKeyExpire(expiredInSeconds);
}

- (BOOL)disableAutoKeyExpire {
    return m_mmkv->disableAutoKeyExpire();
}

- (void)removeValueForKey:(NSString *)key {
    m_mmkv->removeValueForKey(key);
}

- (void)removeValuesForKeys:(NSArray *)arrKeys {
    m_mmkv->removeValuesForKeys(arrKeys);
}

- (void)sync {
    m_mmkv->sync(mmkv::MMKV_SYNC);
}

- (void)async {
    m_mmkv->sync(mmkv::MMKV_ASYNC);
}

- (void)checkContentChanged {
    m_mmkv->checkContentChanged();
}

+ (void)onAppTerminate {
    g_lock->lock();

    // make sure no further call will go into m_mmkv
    [g_instanceDic enumerateKeysAndObjectsUsingBlock:^(id _Nonnull key, MMKV *_Nonnull mmkv, BOOL *_Nonnull stop) {
        mmkv->m_mmkv = nullptr;
    }];
    [g_instanceDic release];
    g_instanceDic = nil;

    [g_basePath release];
    g_basePath = nil;

    [g_groupPath release];
    g_groupPath = nil;

    mmkv::MMKV::onExit();

    g_lock->unlock();
    delete g_lock;
    g_lock = nullptr;
}

static bool g_isAutoCleanUpEnabled = false;
static uint32_t g_maxIdleSeconds = 0;
static dispatch_source_t g_autoCleanUpTimer = nullptr;

+ (void)enableAutoCleanUp:(uint32_t)maxIdleMinutes {
    MMKVInfo("enable auto clean up with maxIdleMinutes:%zu", maxIdleMinutes);
    SCOPED_LOCK(g_lock);

    g_isAutoCleanUpEnabled = true;
    g_maxIdleSeconds = maxIdleMinutes * 60;

    if (!g_autoCleanUpTimer) {
        g_autoCleanUpTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
        dispatch_source_set_event_handler(g_autoCleanUpTimer, ^{
            [MMKV tryAutoCleanUpInstances];
        });
    } else {
        dispatch_suspend(g_autoCleanUpTimer);
    }
    dispatch_source_set_timer(g_autoCleanUpTimer, dispatch_time(DISPATCH_TIME_NOW, g_maxIdleSeconds * NSEC_PER_SEC), g_maxIdleSeconds * NSEC_PER_SEC, 0);
    dispatch_resume(g_autoCleanUpTimer);
}

+ (void)disableAutoCleanUp {
    MMKVInfo("disable auto clean up");
    SCOPED_LOCK(g_lock);

    g_isAutoCleanUpEnabled = false;
    g_maxIdleSeconds = 0;

    if (g_autoCleanUpTimer) {
        dispatch_source_cancel(g_autoCleanUpTimer);
        dispatch_release(g_autoCleanUpTimer);
        g_autoCleanUpTimer = nullptr;
    }
}

+ (void)tryAutoCleanUpInstances {
    SCOPED_LOCK(g_lock);

#ifdef MMKV_IOS
    if (mmkv::MMKV::isInBackground()) {
        MMKVInfo("don't cleanup in background, might just wakeup from suspend");
        return;
    }
#endif

    uint64_t now = [NSDate timeIntervalSinceReferenceDate];
    NSMutableArray *arrKeys = [NSMutableArray array];
    [g_instanceDic enumerateKeysAndObjectsUsingBlock:^(id _Nonnull key, id _Nonnull obj, BOOL *_Nonnull stop) {
        auto mmkv = (MMKV *) obj;
        if (mmkv->m_lastAccessTime + g_maxIdleSeconds <= now && mmkv.retainCount == 1) {
            [arrKeys addObject:key];
        }
    }];
    if (arrKeys.count > 0) {
        auto msg = [NSString stringWithFormat:@"auto cleanup mmkv %@", arrKeys];
        MMKVInfo(msg.UTF8String);
        [g_instanceDic removeObjectsForKeys:arrKeys];
    }
}

+ (NSString *)mmkvBasePath {
    if (g_basePath.length > 0) {
        return g_basePath;
    }

#if TARGET_OS_TV
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
#else
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
#endif
    NSString *documentPath = (NSString *) [paths firstObject];
    if ([documentPath length] > 0) {
        g_basePath = [[documentPath stringByAppendingPathComponent:@"mmkv"] retain];
        return g_basePath;
    } else {
        return @"";
    }
}

+ (void)setMMKVBasePath:(NSString *)basePath {
    if (basePath.length > 0) {
        [g_basePath release];
        g_basePath = [basePath retain];
        [MMKV initializeMMKV:basePath];

        // still warn about it
        g_hasCalledInitializeMMKV = NO;

        MMKVInfo("set MMKV base path to: %@", g_basePath);
    }
}

- (void)setValue:(nullable id)value forKey:(NSString *)key {
    [super setValue:value forKey:key];
}

- (void)setValue:(nullable id)value forKeyPath:(NSString *)keyPath {
    [super setValue:value forKeyPath:keyPath];
}

static NSString *md5(NSString *value) {
    uint8_t md[MD5_DIGEST_LENGTH] = {};
    char tmp[3] = {}, buf[33] = {};
    auto data = [value dataUsingEncoding:NSUTF8StringEncoding];
    openssl::MD5((uint8_t *) data.bytes, data.length, md);
    for (auto ch : md) {
        snprintf(tmp, sizeof(tmp), "%2.2x", ch);
        strcat(buf, tmp);
    }
    return [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
}

+ (NSString *)mmapKeyWithMMapID:(NSString *)mmapID rootPath:(nullable NSString *)rootPath {
    NSString *string = nil;
    if ([rootPath length] > 0 && [rootPath isEqualToString:[MMKV mmkvBasePath]] == NO) {
        string = md5([rootPath stringByAppendingPathComponent:mmapID]);
    } else {
        string = mmapID;
    }
    MMKVDebug("mmapKey: %@", string);
    return string;
}

+ (BOOL)isFileValid:(NSString *)mmapID {
    return [self isFileValid:mmapID rootPath:nil];
}

+ (BOOL)isFileValid:(NSString *)mmapID rootPath:(nullable NSString *)path {
    if (mmapID.length > 0) {
        if (path.length > 0) {
            string rootPath(path.UTF8String);
            return mmkv::MMKV::isFileValid(mmapID.UTF8String, &rootPath);
        } else {
            return mmkv::MMKV::isFileValid(mmapID.UTF8String, nullptr);
        }
    }
    return NO;
}

#pragma mark - backup & restore

+ (BOOL)backupOneMMKV:(NSString *)mmapID rootPath:(nullable NSString *)path toDirectory:(NSString *)dstDir {
    if (path.length > 0) {
        string rootPath(path.UTF8String);
        return mmkv::MMKV::backupOneToDirectory(mmapID.UTF8String, dstDir.UTF8String, &rootPath);
    }
    return mmkv::MMKV::backupOneToDirectory(mmapID.UTF8String, dstDir.UTF8String);
}

+ (BOOL)restoreOneMMKV:(NSString *)mmapID rootPath:(nullable NSString *)path fromDirectory:(NSString *)srcDir {
    if (path.length > 0) {
        string rootPath(path.UTF8String);
        return mmkv::MMKV::restoreOneFromDirectory(mmapID.UTF8String, srcDir.UTF8String, &rootPath);
    }
    return mmkv::MMKV::restoreOneFromDirectory(mmapID.UTF8String, srcDir.UTF8String);
}

+ (size_t)backupAll:(nullable NSString *)path toDirectory:(NSString *)dstDir {
    if (path.length > 0) {
        string rootPath(path.UTF8String);
        return mmkv::MMKV::backupAllToDirectory(dstDir.UTF8String, &rootPath);
    }
    return mmkv::MMKV::backupAllToDirectory(dstDir.UTF8String);
}

+ (size_t)restoreAll:(nullable NSString *)path fromDirectory:(NSString *)srcDir {
    if (path.length > 0) {
        string rootPath(path.UTF8String);
        return mmkv::MMKV::restoreAllFromDirectory(srcDir.UTF8String, &rootPath);
    }
    return mmkv::MMKV::restoreAllFromDirectory(srcDir.UTF8String);
}

+ (BOOL)backupMultiProcessMMKV:(NSString *)mmapID toDirectory:(NSString *)dstDir {
    if (!g_groupPath) {
        MMKVError("Backup a multi-process MMKV [%@] without setting groupDir makes no sense", mmapID);
        MMKV_ASSERT(0);
    }
    return [MMKV backupOneMMKV:mmapID rootPath:g_groupPath toDirectory:dstDir];
}

+ (BOOL)restoreMultiProcessMMKV:(NSString *)mmapID fromDirectory:(NSString *)srcDir {
    if (!g_groupPath) {
        MMKVError("Restore a multi-process MMKV [%@] without setting groupDir makes no sense", mmapID);
        MMKV_ASSERT(0);
    }
    return [MMKV restoreOneMMKV:mmapID rootPath:g_groupPath fromDirectory:srcDir];
}

+ (size_t)backupAllMultiProcessToDirectory:(NSString *)dstDir {
    if (!g_groupPath) {
        MMKVError("Backup multi-process MMKV without setting groupDir makes no sense.");
        MMKV_ASSERT(0);
    }
    return [MMKV backupAll:g_groupPath toDirectory:dstDir];
}

+ (size_t)restoreAllMultiProcessFromDirectory:(NSString *)srcDir {
    if (!g_groupPath) {
        MMKVError("Restore multi-process MMKV without setting groupDir makes no sense.");
        MMKV_ASSERT(0);
    }
    return [MMKV restoreAll:g_groupPath fromDirectory:srcDir];
}

#pragma mark - handler

+ (void)registerHandler:(id<MMKVHandler>)handler {
    SCOPED_LOCK(g_lock);
    g_callbackHandler = handler;

    if ([g_callbackHandler respondsToSelector:@selector(mmkvLogWithLevel:file:line:func:message:)]) {
        g_isLogRedirecting = true;
        mmkv::MMKV::registerLogHandler(LogHandler);
    }
    if ([g_callbackHandler respondsToSelector:@selector(onMMKVCRCCheckFail:)] ||
        [g_callbackHandler respondsToSelector:@selector(onMMKVFileLengthError:)]) {
        mmkv::MMKV::registerErrorHandler(ErrorHandler);
    }
    if ([g_callbackHandler respondsToSelector:@selector(onMMKVContentChange:)]) {
        mmkv::MMKV::registerContentChangeHandler(ContentChangeHandler);
    }
}

+ (void)unregiserHandler {
    SCOPED_LOCK(g_lock);

    g_isLogRedirecting = false;
    g_callbackHandler = nil;

    mmkv::MMKV::unRegisterLogHandler();
    mmkv::MMKV::unRegisterErrorHandler();
    mmkv::MMKV::unRegisterContentChangeHandler();
}

+ (void)setLogLevel:(MMKVLogLevel)logLevel {
    mmkv::MMKV::setLogLevel((mmkv::MMKVLogLevel) logLevel);
}

- (uint32_t)migrateFromUserDefaults:(NSUserDefaults *)userDaults {
    @autoreleasepool {
        NSDictionary *dic = [userDaults dictionaryRepresentation];
        if (dic.count <= 0) {
            MMKVInfo("migrate data fail, userDaults is nil or empty");
            return 0;
        }
        __block uint32_t count = 0;
        [dic enumerateKeysAndObjectsUsingBlock:^(id _Nonnull key, id _Nonnull obj, BOOL *_Nonnull stop) {
            if ([key isKindOfClass:[NSString class]]) {
                NSString *stringKey = key;
                if ([MMKV tranlateData:obj key:stringKey kv:self]) {
                    count++;
                }
            } else {
                MMKVWarning("unknown type of key:%@", key);
            }
        }];
        return count;
    }
}

+ (BOOL)tranlateData:(id)obj key:(NSString *)key kv:(MMKV *)kv {
    if ([obj isKindOfClass:[NSString class]]) {
        return [kv setString:obj forKey:key];
    } else if ([obj isKindOfClass:[NSData class]]) {
        return [kv setData:obj forKey:key];
    } else if ([obj isKindOfClass:[NSDate class]]) {
        return [kv setDate:obj forKey:key];
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        NSNumber *num = obj;
        CFNumberType numberType = CFNumberGetType((CFNumberRef) obj);
        switch (numberType) {
            case kCFNumberCharType:
            case kCFNumberSInt8Type:
            case kCFNumberSInt16Type:
            case kCFNumberSInt32Type:
            case kCFNumberIntType:
            case kCFNumberShortType:
                return [kv setInt32:num.intValue forKey:key];
            case kCFNumberSInt64Type:
            case kCFNumberLongType:
            case kCFNumberNSIntegerType:
            case kCFNumberLongLongType:
                return [kv setInt64:num.longLongValue forKey:key];
            case kCFNumberFloat32Type:
                return [kv setFloat:num.floatValue forKey:key];
            case kCFNumberFloat64Type:
            case kCFNumberDoubleType:
                return [kv setDouble:num.doubleValue forKey:key];
            default:
                MMKVWarning("unknown number type:%ld, key:%@", (long) numberType, key);
                return NO;
        }
    } else if ([obj isKindOfClass:[NSArray class]] || [obj isKindOfClass:[NSDictionary class]]) {
        return [kv setObject:obj forKey:key];
    } else {
        MMKVWarning("unknown type of key:%@", key);
    }
    return NO;
}

@end

#pragma makr - callbacks

static void LogHandler(mmkv::MMKVLogLevel level, const char *file, int line, const char *function, NSString *message) {
    [g_callbackHandler mmkvLogWithLevel:(MMKVLogLevel) level file:file line:line func:function message:message];
}

static mmkv::MMKVRecoverStrategic ErrorHandler(const string &mmapID, mmkv::MMKVErrorType errorType) {
    if (errorType == mmkv::MMKVCRCCheckFail) {
        if ([g_callbackHandler respondsToSelector:@selector(onMMKVCRCCheckFail:)]) {
            auto ret = [g_callbackHandler onMMKVCRCCheckFail:[NSString stringWithUTF8String:mmapID.c_str()]];
            return (mmkv::MMKVRecoverStrategic) ret;
        }
    } else {
        if ([g_callbackHandler respondsToSelector:@selector(onMMKVFileLengthError:)]) {
            auto ret = [g_callbackHandler onMMKVFileLengthError:[NSString stringWithUTF8String:mmapID.c_str()]];
            return (mmkv::MMKVRecoverStrategic) ret;
        }
    }
    return mmkv::OnErrorDiscard;
}

static void ContentChangeHandler(const string &mmapID) {
    if ([g_callbackHandler respondsToSelector:@selector(onMMKVContentChange:)]) {
        [g_callbackHandler onMMKVContentChange:[NSString stringWithUTF8String:mmapID.c_str()]];
    }
}
