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

#import "MMKV.h"
#import <Core/MMKV.h>
#import <Core/ScopedLock.hpp>
#import <Core/aes/openssl/openssl_md5.h>

#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
#import <UIKit/UIKit.h>
#endif

#define MMKVDebug NSLog
#define MMKVInfo NSLog
#define MMKVWarning NSLog
#define MMKVError NSLog

using namespace std;

static NSMutableDictionary *g_instanceDic;
static mmkv::ThreadLock g_lock;
id<MMKVHandler> g_callbackHandler;
bool g_isLogRedirecting = false;
static NSString *g_basePath = nil;

static NSString *md5(NSString *value);
static void LogHandler(mmkv::MMKVLogLevel level, const char *file, int line, const char *function, NSString *message);
static mmkv::MMKVRecoverStrategic ErrorHandler(const std::string &mmapID, mmkv::MMKVErrorType errorType);
static void ContentChangeHandler(const std::string &mmapID);

enum : bool {
    KeepSequence = false,
    IncreaseSequence = true,
};

@implementation MMKV {
    NSString *m_mmapID;
    mmkv::MMKV *m_mmkv;
}

#pragma mark - init

+ (void)initialize {
    if (self == MMKV.class) {
        g_instanceDic = [NSMutableDictionary dictionary];
        g_lock = mmkv::ThreadLock();
        g_lock.initialize();

        // protect from some old code that don't call +initializeMMKV:
        mmkv::MMKV::minimalInit([self mmkvBasePath].UTF8String);

#ifdef MMKV_IOS
        auto appState = [UIApplication sharedApplication].applicationState;
        auto isInBackground = (appState == UIApplicationStateBackground);
        mmkv::MMKV::setIsInBackground(isInBackground);
        MMKVInfo(@"appState:%ld", (long) appState);

        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didEnterBackground) name:UIApplicationDidEnterBackgroundNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didBecomeActive) name:UIApplicationDidBecomeActiveNotification object:nil];
#endif
    }
}

+ (NSString *)initializeMMKV:(nullable NSString *)rootDir {
    return [MMKV initializeMMKV:rootDir logLevel:MMKVLogInfo];
}

static BOOL g_hasCalledInitializeMMKV = NO;

+ (NSString *)initializeMMKV:(nullable NSString *)rootDir logLevel:(MMKVLogLevel)logLevel {
    if (g_hasCalledInitializeMMKV) {
        MMKVWarning(@"already called +initializeMMKV before, ignore this request");
        return [self mmkvBasePath];
    }

    g_basePath = (rootDir != nil) ? rootDir : [self mmkvBasePath];
    mmkv::MMKV::initializeMMKV(g_basePath.UTF8String, (mmkv::MMKVLogLevel) logLevel);

    MMKVInfo(@"pagesize:%zu", mmkv::DEFAULT_MMAP_SIZE);

    g_hasCalledInitializeMMKV = YES;

    return [self mmkvBasePath];
}

// a generic purpose instance
+ (instancetype)defaultMMKV {
    return [MMKV mmkvWithID:@"" DEFAULT_MMAP_ID];
}

// any unique ID (com.tencent.xin.pay, etc)
+ (instancetype)mmkvWithID:(NSString *)mmapID {
    return [MMKV mmkvWithID:mmapID cryptKey:nil];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey {
    return [MMKV mmkvWithID:mmapID cryptKey:cryptKey relativePath:nil];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID relativePath:(nullable NSString *)relativePath {
    return [MMKV mmkvWithID:mmapID cryptKey:nil relativePath:relativePath];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey relativePath:(nullable NSString *)relativePath {
    if (!g_hasCalledInitializeMMKV) {
        MMKVError(@"MMKV not initialized properly, must call +initializeMMKV: in main thread before calling any other MMKV methods");
    }
    if (mmapID.length <= 0) {
        return nil;
    }

    SCOPEDLOCK(g_lock);

    NSString *kvKey = [MMKV mmapKeyWithMMapID:mmapID relativePath:relativePath];
    MMKV *kv = [g_instanceDic objectForKey:kvKey];
    if (kv == nil) {
        kv = [[MMKV alloc] initWithMMapID:mmapID cryptKey:cryptKey relativePath:relativePath];
        [g_instanceDic setObject:kv forKey:kvKey];
    }
    return kv;
}

- (instancetype)initWithMMapID:(NSString *)kvKey cryptKey:(NSData *)cryptKey relativePath:(NSString *)relativePath {
    if (self = [super init]) {
        string pathTmp;
        if (relativePath.length > 0) {
            pathTmp = relativePath.UTF8String;
        }
        string cryptKeyTmp;
        if (cryptKey.length > 0) {
            cryptKeyTmp = string((char *) cryptKey.bytes, cryptKey.length);
        }
        string *relativePathPtr = pathTmp.empty() ? nullptr : &pathTmp;
        string *cryptKeyPtr = cryptKeyTmp.empty() ? nullptr : &cryptKeyTmp;
        m_mmkv = mmkv::MMKV::mmkvWithID(kvKey.UTF8String, mmkv::MMKV_SINGLE_PROCESS, cryptKeyPtr, relativePathPtr);
        m_mmapID = [NSString stringWithUTF8String:m_mmkv->mmapID().c_str()];

#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(onMemoryWarning)
                                                     name:UIApplicationDidReceiveMemoryWarningNotification
                                                   object:nil];
#endif
    }
    return self;
}

- (void)dealloc {
    [self clearMemoryCache];

    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Application state

#ifdef MMKV_IOS
- (void)onMemoryWarning {
    MMKVInfo(@"cleaning on memory warning %@", m_mmapID);

    [self clearMemoryCache];
}

+ (void)didEnterBackground {
    mmkv::MMKV::setIsInBackground(true);
    MMKVInfo(@"isInBackground:%d", true);
}

+ (void)didBecomeActive {
    mmkv::MMKV::setIsInBackground(false);
    MMKVInfo(@"isInBackground:%d", false);
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
    SCOPEDLOCK(g_lock);
    MMKVInfo(@"closing %@", m_mmapID);

    m_mmkv->close();
    m_mmkv = nullptr;

    [g_instanceDic removeObjectForKey:m_mmapID];
}

- (void)trim {
    m_mmkv->trim();
}

#pragma mark - encryption & decryption

- (nullable NSData *)cryptKey {
    auto str = m_mmkv->cryptKey();
    return [NSData dataWithBytes:str.data() length:str.length()];
}

- (BOOL)reKey:(NSData *)newKey {
    string key;
    if (newKey.length > 0) {
        key = string((char *) newKey.bytes, newKey.length);
    }
    return m_mmkv->reKey(key);
}

#pragma mark - set & get

- (BOOL)setObject:(nullable NSObject<NSCoding> *)object forKey:(NSString *)key {
    return m_mmkv->set(object, key);
}

- (BOOL)setBool:(BOOL)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setInt32:(int32_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setInt64:(int64_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setFloat:(float)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setDouble:(double)value forKey:(NSString *)key {
    return m_mmkv->set(value, key);
}

- (BOOL)setString:(NSString *)value forKey:(NSString *)key {
    return [self setObject:value forKey:key];
}

- (BOOL)setDate:(NSDate *)value forKey:(NSString *)key {
    return [self setObject:value forKey:key];
}

- (BOOL)setData:(NSData *)value forKey:(NSString *)key {
    return [self setObject:value forKey:key];
}

- (id)getObjectOfClass:(Class)cls forKey:(NSString *)key {
    return m_mmkv->getObject(key, cls);
}

- (BOOL)getBoolForKey:(NSString *)key {
    return [self getBoolForKey:key defaultValue:FALSE];
}
- (BOOL)getBoolForKey:(NSString *)key defaultValue:(BOOL)defaultValue {
    return m_mmkv->getBool(key, defaultValue);
}

- (int32_t)getInt32ForKey:(NSString *)key {
    return [self getInt32ForKey:key defaultValue:0];
}
- (int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue {
    return m_mmkv->getInt32(key, defaultValue);
}

- (uint32_t)getUInt32ForKey:(NSString *)key {
    return [self getUInt32ForKey:key defaultValue:0];
}
- (uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue {
    return m_mmkv->getUInt32(key, defaultValue);
}

- (int64_t)getInt64ForKey:(NSString *)key {
    return [self getInt64ForKey:key defaultValue:0];
}
- (int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue {
    return m_mmkv->getInt64(key, defaultValue);
}

- (uint64_t)getUInt64ForKey:(NSString *)key {
    return [self getUInt64ForKey:key defaultValue:0];
}
- (uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue {
    return m_mmkv->getUInt64(key, defaultValue);
}

- (float)getFloatForKey:(NSString *)key {
    return [self getFloatForKey:key defaultValue:0];
}
- (float)getFloatForKey:(NSString *)key defaultValue:(float)defaultValue {
    return m_mmkv->getFloat(key, defaultValue);
}

- (double)getDoubleForKey:(NSString *)key {
    return [self getDoubleForKey:key defaultValue:0];
}
- (double)getDoubleForKey:(NSString *)key defaultValue:(double)defaultValue {
    return m_mmkv->getDouble(key, defaultValue);
}

- (nullable NSString *)getStringForKey:(NSString *)key {
    return [self getStringForKey:key defaultValue:nil];
}
- (nullable NSString *)getStringForKey:(NSString *)key defaultValue:(nullable NSString *)defaultValue {
    if (key.length <= 0) {
        return defaultValue;
    }
    NSString *valueString = [self getObjectOfClass:NSString.class forKey:key];
    if (!valueString) {
        valueString = defaultValue;
    }
    return valueString;
}

- (nullable NSDate *)getDateForKey:(NSString *)key {
    return [self getDateForKey:key defaultValue:nil];
}
- (nullable NSDate *)getDateForKey:(NSString *)key defaultValue:(nullable NSDate *)defaultValue {
    if (key.length <= 0) {
        return defaultValue;
    }
    NSDate *valueDate = [self getObjectOfClass:NSDate.class forKey:key];
    if (!valueDate) {
        valueDate = defaultValue;
    }
    return valueDate;
}

- (nullable NSData *)getDataForKey:(NSString *)key {
    return [self getDataForKey:key defaultValue:nil];
}
- (nullable NSData *)getDataForKey:(NSString *)key defaultValue:(nullable NSData *)defaultValue {
    if (key.length <= 0) {
        return defaultValue;
    }
    NSData *valueData = [self getObjectOfClass:NSData.class forKey:key];
    if (!valueData) {
        valueData = defaultValue;
    }
    return valueData;
}

- (size_t)getValueSizeForKey:(NSString *)key NS_SWIFT_NAME(valueSize(forKey:)) {
    return m_mmkv->getValueSize(key, false);
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

- (void)enumerateKeys:(void (^)(NSString *key, BOOL *stop))block {
    m_mmkv->enumerateKeys(block);
}

- (NSArray *)allKeys {
    return m_mmkv->allKeys();
}

- (void)removeValueForKey:(NSString *)key {
    m_mmkv->removeValueForKey(key);
}

- (void)removeValuesForKeys:(NSArray *)arrKeys {
    m_mmkv->removeValuesForKeys(arrKeys);
}

#pragma mark - Boring stuff

- (void)sync {
    m_mmkv->sync(MMKV_SYNC);
}

- (void)async {
    m_mmkv->sync(MMKV_ASYNC);
}

+ (NSString *)mmkvBasePath {
    if (g_basePath.length > 0) {
        return g_basePath;
    }

    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentPath = (NSString *) [paths firstObject];
    if ([documentPath length] > 0) {
        g_basePath = [documentPath stringByAppendingPathComponent:@"mmkv"];
        return g_basePath;
    } else {
        return @"";
    }
}

+ (void)setMMKVBasePath:(NSString *)basePath {
    if (basePath.length > 0) {
        g_basePath = basePath;
        [MMKV initializeMMKV:basePath];

        // still warn about it
        g_hasCalledInitializeMMKV = NO;

        MMKVInfo(@"set MMKV base path to: %@", g_basePath);
    }
}

+ (NSString *)mmapKeyWithMMapID:(NSString *)mmapID relativePath:(nullable NSString *)relativePath {
    NSString *string = nil;
    if ([relativePath length] > 0 && [relativePath isEqualToString:[MMKV mmkvBasePath]] == NO) {
        string = md5([relativePath stringByAppendingPathComponent:mmapID]);
    } else {
        string = mmapID;
    }
    MMKVInfo(@"mmapKey: %@", string);
    return string;
}

+ (BOOL)isFileValid:(NSString *)mmapID {
    return [self isFileValid:mmapID relativePath:nil];
}

+ (BOOL)isFileValid:(NSString *)mmapID relativePath:(nullable NSString *)path {
    if (mmapID.length > 0) {
        if (path.length > 0) {
            string relativePath(path.UTF8String);
            return mmkv::MMKV::isFileValid(mmapID.UTF8String, &relativePath);
        } else {
            return mmkv::MMKV::isFileValid(mmapID.UTF8String, nullptr);
        }
    }
    return NO;
}

+ (void)registerHandler:(id<MMKVHandler>)handler {
    SCOPEDLOCK(g_lock);
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
    SCOPEDLOCK(g_lock);

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
    NSDictionary *dic = [userDaults dictionaryRepresentation];
    if (dic.count <= 0) {
        MMKVInfo(@"migrate data fail, userDaults is nil or empty");
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
            MMKVWarning(@"unknown type of key:%@", key);
        }
    }];
    return count;
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
                MMKVWarning(@"unknown number type:%ld, key:%@", (long) numberType, key);
                return NO;
        }
    } else if ([obj isKindOfClass:[NSArray class]] || [obj isKindOfClass:[NSDictionary class]]) {
        return [kv setObject:obj forKey:key];
    } else {
        MMKVWarning(@"unknown type of key:%@", key);
    }
    return NO;
}

@end

static NSString *md5(NSString *value) {
    unsigned char md[MD5_DIGEST_LENGTH] = {0};
    char tmp[3] = {0}, buf[33] = {0};
    auto data = [value dataUsingEncoding:NSUTF8StringEncoding];
    openssl::MD5((unsigned char *) data.bytes, data.length, md);
    for (auto ch : md) {
        sprintf(tmp, "%2.2x", ch);
        strcat(buf, tmp);
    }
    return [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
}

static inline const char *MMKVLogLevelDesc(MMKVLogLevel level) {
    switch (level) {
        case MMKVLogDebug:
            return "D";
        case MMKVLogInfo:
            return "I";
        case MMKVLogWarning:
            return "W";
        case MMKVLogError:
            return "E";
        default:
            return "N";
    }
}

static void LogHandler(mmkv::MMKVLogLevel level, const char *file, int line, const char *function, NSString *message) {
    if (g_isLogRedirecting) {
        [g_callbackHandler mmkvLogWithLevel:(MMKVLogLevel) level file:file line:line func:function message:message];
    } else {
        NSLog(@"[%s] <%s:%d::%s> %@", MMKVLogLevelDesc((MMKVLogLevel) level), file, line, function, message);
    }
}

static mmkv::MMKVRecoverStrategic ErrorHandler(const std::string &mmapID, mmkv::MMKVErrorType errorType) {
    return mmkv::OnErrorDiscard;
}

static void ContentChangeHandler(const std::string &mmapID) {
}
