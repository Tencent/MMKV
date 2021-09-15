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

#import "MMKVHandler.h"

typedef NS_ENUM(NSUInteger, MMKVMode) {
    MMKVSingleProcess = 0x1,
    MMKVMultiProcess = 0x2,
};

@interface MMKV : NSObject

NS_ASSUME_NONNULL_BEGIN

/// call this in main thread, before calling any other MMKV methods
/// @param rootDir the root dir of MMKV, passing nil defaults to {NSDocumentDirectory}/mmkv
/// @return root dir of MMKV
+ (NSString *)initializeMMKV:(nullable NSString *)rootDir NS_SWIFT_NAME(initialize(rootDir:));

/// call this in main thread, before calling any other MMKV methods
/// @param rootDir the root dir of MMKV, passing nil defaults to {NSDocumentDirectory}/mmkv
/// @param logLevel MMKVLogInfo by default, MMKVLogNone to disable all logging
/// @return root dir of MMKV
+ (NSString *)initializeMMKV:(nullable NSString *)rootDir logLevel:(MMKVLogLevel)logLevel NS_SWIFT_NAME(initialize(rootDir:logLevel:));

/// call this in main thread, before calling any other MMKV methods
/// @param rootDir the root dir of MMKV, passing nil defaults to {NSDocumentDirectory}/mmkv
/// @param groupDir the root dir of multi-process MMKV, MMKV with MMKVMultiProcess mode will be stored in groupDir/mmkv
/// @param logLevel MMKVLogInfo by default, MMKVLogNone to disable all logging
/// @return root dir of MMKV
+ (NSString *)initializeMMKV:(nullable NSString *)rootDir groupDir:(NSString *)groupDir logLevel:(MMKVLogLevel)logLevel NS_SWIFT_NAME(initialize(rootDir:groupDir:logLevel:));

/// a generic purpose instance (in MMKVSingleProcess mode)
+ (nullable instancetype)defaultMMKV;

/// an encrypted generic purpose instance (in MMKVSingleProcess mode)
+ (nullable instancetype)defaultMMKVWithCryptKey:(nullable NSData *)cryptKey;

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID NS_SWIFT_NAME(init(mmapID:));

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
/// @param mode MMKVMultiProcess for multi-process MMKV
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID mode:(MMKVMode)mode NS_SWIFT_NAME(init(mmapID:mode:));

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
/// @param cryptKey 16 bytes at most
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey NS_SWIFT_NAME(init(mmapID:cryptKey:));

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
/// @param cryptKey 16 bytes at most
/// @param mode MMKVMultiProcess for multi-process MMKV
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey mode:(MMKVMode)mode NS_SWIFT_NAME(init(mmapID:cryptKey:mode:));

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
/// @param relativePath custom path of the file, `NSDocumentDirectory/mmkv` by default
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID relativePath:(nullable NSString *)relativePath NS_SWIFT_NAME(init(mmapID:relativePath:)) __attribute__((deprecated("use +mmkvWithID:rootPath: instead")));

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
/// @param rootPath custom path of the file, `NSDocumentDirectory/mmkv` by default
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID rootPath:(nullable NSString *)rootPath NS_SWIFT_NAME(init(mmapID:rootPath:));

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
/// @param cryptKey 16 bytes at most
/// @param relativePath custom path of the file, `NSDocumentDirectory/mmkv` by default
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey relativePath:(nullable NSString *)relativePath NS_SWIFT_NAME(init(mmapID:cryptKey:relativePath:)) __attribute__((deprecated("use +mmkvWithID:cryptKey:rootPath: instead")));

/// @param mmapID any unique ID (com.tencent.xin.pay, etc), if you want a per-user mmkv, you could merge user-id within mmapID
/// @param cryptKey 16 bytes at most
/// @param rootPath custom path of the file, `NSDocumentDirectory/mmkv` by default
+ (nullable instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(nullable NSData *)cryptKey rootPath:(nullable NSString *)rootPath NS_SWIFT_NAME(init(mmapID:cryptKey:rootPath:));

/// you can call this on applicationWillTerminate, it's totally fine if you don't call
+ (void)onAppTerminate;

+ (NSString *)mmkvBasePath;

/// if you want to change the base path, do it BEFORE getting any MMKV instance
/// otherwise the behavior is undefined
+ (void)setMMKVBasePath:(NSString *)basePath __attribute__((deprecated("use +initializeMMKV: instead", "+initializeMMKV:")));

- (NSString *)mmapID;

/// transform plain text into encrypted text, or vice versa by passing newKey = nil
/// you can change existing crypt key with different key
- (BOOL)reKey:(nullable NSData *)newKey NS_SWIFT_NAME(reset(cryptKey:));
- (nullable NSData *)cryptKey;

/// just reset cryptKey (will not encrypt or decrypt anything)
/// usually you should call this method after other process reKey() the multi-process mmkv
- (void)checkReSetCryptKey:(nullable NSData *)cryptKey NS_SWIFT_NAME(checkReSet(cryptKey:));

- (BOOL)setObject:(nullable NSObject<NSCoding> *)object forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setBool:(BOOL)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setInt32:(int32_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setInt64:(int64_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setFloat:(float)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setDouble:(double)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setString:(NSString *)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setDate:(NSDate *)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (BOOL)setData:(NSData *)value forKey:(NSString *)key NS_SWIFT_NAME(set(_:forKey:));

- (nullable id)getObjectOfClass:(Class)cls forKey:(NSString *)key NS_SWIFT_NAME(object(of:forKey:));

- (BOOL)getBoolForKey:(NSString *)key __attribute__((swift_name("bool(forKey:)")));
- (BOOL)getBoolForKey:(NSString *)key defaultValue:(BOOL)defaultValue __attribute__((swift_name("bool(forKey:defaultValue:)")));

- (int32_t)getInt32ForKey:(NSString *)key NS_SWIFT_NAME(int32(forKey:));
- (int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue NS_SWIFT_NAME(int32(forKey:defaultValue:));

- (uint32_t)getUInt32ForKey:(NSString *)key NS_SWIFT_NAME(uint32(forKey:));
- (uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue NS_SWIFT_NAME(uint32(forKey:defaultValue:));

- (int64_t)getInt64ForKey:(NSString *)key NS_SWIFT_NAME(int64(forKey:));
- (int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue NS_SWIFT_NAME(int64(forKey:defaultValue:));

- (uint64_t)getUInt64ForKey:(NSString *)key NS_SWIFT_NAME(uint64(forKey:));
- (uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue NS_SWIFT_NAME(uint64(forKey:defaultValue:));

- (float)getFloatForKey:(NSString *)key NS_SWIFT_NAME(float(forKey:));
- (float)getFloatForKey:(NSString *)key defaultValue:(float)defaultValue NS_SWIFT_NAME(float(forKey:defaultValue:));

- (double)getDoubleForKey:(NSString *)key NS_SWIFT_NAME(double(forKey:));
- (double)getDoubleForKey:(NSString *)key defaultValue:(double)defaultValue NS_SWIFT_NAME(double(forKey:defaultValue:));

- (nullable NSString *)getStringForKey:(NSString *)key NS_SWIFT_NAME(string(forKey:));
- (nullable NSString *)getStringForKey:(NSString *)key defaultValue:(nullable NSString *)defaultValue NS_SWIFT_NAME(string(forKey:defaultValue:));

- (nullable NSDate *)getDateForKey:(NSString *)key NS_SWIFT_NAME(date(forKey:));
- (nullable NSDate *)getDateForKey:(NSString *)key defaultValue:(nullable NSDate *)defaultValue NS_SWIFT_NAME(date(forKey:defaultValue:));

- (nullable NSData *)getDataForKey:(NSString *)key NS_SWIFT_NAME(data(forKey:));
- (nullable NSData *)getDataForKey:(NSString *)key defaultValue:(nullable NSData *)defaultValue NS_SWIFT_NAME(data(forKey:defaultValue:));

// return the actual size consumption of the key's value
// Note: might be a little bigger than value's length
- (size_t)getValueSizeForKey:(NSString *)key actualSize:(BOOL)actualSize NS_SWIFT_NAME(valueSize(forKey:actualSize:));

/// @return size written into buffer
/// @return -1 on any error
- (int32_t)writeValueForKey:(NSString *)key toBuffer:(NSMutableData *)buffer NS_SWIFT_NAME(writeValue(forKey:buffer:));

- (BOOL)containsKey:(NSString *)key NS_SWIFT_NAME(contains(key:));

- (size_t)count;

- (size_t)totalSize;

- (size_t)actualSize;

+ (size_t)pageSize;

+ (NSString *)version;

- (void)enumerateKeys:(void (^)(NSString *key, BOOL *stop))block;
- (NSArray *)allKeys;

- (void)removeValueForKey:(NSString *)key NS_SWIFT_NAME(removeValue(forKey:));

- (void)removeValuesForKeys:(NSArray<NSString *> *)arrKeys NS_SWIFT_NAME(removeValues(forKeys:));

- (void)clearAll;

// MMKV's size won't reduce after deleting key-values
// call this method after lots of deleting if you care about disk usage
// note that `clearAll` has the similar effect of `trim`
- (void)trim;

/// call this method if the instance is no longer needed in the near future
/// any subsequent call to the instance is undefined behavior
- (void)close;

/// call this method if you are facing memory-warning
/// any subsequent call to the instance will load all key-values from file again
- (void)clearMemoryCache;

/// enable auto cleanup items that not been accessed recently
/// disable by default
/// note: if an item is strong referenced by outside, it won't be cleanup
+ (void)enableAutoCleanUp:(uint32_t)maxIdleMinutes NS_SWIFT_NAME(enableAutoCleanUp(maxIdleMinutes:));

+ (void)disableAutoCleanUp;

/// you don't need to call this, really, I mean it
/// unless you worry about running out of battery
- (void)sync;
- (void)async;

/// backup one MMKV instance to dstDir
/// @param mmapID the MMKV ID to backup
/// @param rootPath the customize root path of the MMKV, if null then backup from the root dir of MMKV
/// @param dstDir the backup destination directory
+ (BOOL) backupOneMMKV:(NSString*)mmapID rootPath:(nullable NSString *)rootPath toDirectory:(NSString*)dstDir  NS_SWIFT_NAME(backup(mmapID:rootPath:dstDir:));

/// restore one MMKV instance from srcDir
/// @param mmapID the MMKV ID to restore
/// @param rootPath the customize root path of the MMKV, if null then restore to the root dir of MMKV
/// @param srcDir the restore source directory
+ (BOOL) restoreOneMMKV:(NSString*)mmapID rootPath:(nullable NSString *)rootPath fromDirectory:(NSString*)srcDir NS_SWIFT_NAME(restore(mmapID:rootPath:srcDir:));

/// backup all MMKV instance to dstDir
/// @param rootPath the customize root path of the MMKV
/// @param dstDir the backup destination directory
/// @return count of MMKV successfully backuped
+ (size_t) backupAll:(nullable NSString *)rootPath toDirectory:(NSString*)dstDir NS_SWIFT_NAME(backupAll(rootPath:dstDir:));

/// restore all MMKV instance from srcDir
/// @param rootPath the customize root path of the MMKV
/// @param srcDir the restore source directory
/// @return count of MMKV successfully restored
+ (size_t) restoreAll:(nullable NSString *)rootPath fromDirectory:(NSString*)srcDir NS_SWIFT_NAME(restoreAll(rootPath:srcDir:));

/// backup one MMKVMultiProcess MMKV instance to dstDir
/// @param mmapID the MMKV ID to backup
/// @param dstDir the backup destination directory
+ (BOOL)backupMultiProcessMMKV:(NSString *)mmapID toDirectory:(NSString*)dstDir NS_SWIFT_NAME(backupMultiProcess(mmapID:dstDir:));

/// restore one MMKVMultiProcess MMKV instance from srcDir
/// @param mmapID the MMKV ID to restore
/// @param srcDir the restore source directory
+ (BOOL) restoreMultiProcessMMKV:(NSString*)mmapID fromDirectory:(NSString*)srcDir NS_SWIFT_NAME(restoreMultiProcess(mmapID:srcDir:));

/// backup all MMKVMultiProcess MMKV instance to dstDir
/// @param dstDir the backup destination directory
/// @return count of MMKV successfully backuped
+ (size_t) backupAllMultiProcessToDirectory:(NSString*)dstDir NS_SWIFT_NAME(backupAllMultiProcess(dstDir:));

/// restore all MMKVMultiProcess MMKV instance from srcDir
/// @param srcDir the restore source directory
/// @return count of MMKV successfully restored
+ (size_t) restoreAllMultiProcessFromDirectory:(NSString*)srcDir NS_SWIFT_NAME(restoreAllMultiProcess(srcDir:));

/// check if content changed by other process
- (void)checkContentChanged;

+ (void)registerHandler:(id<MMKVHandler>)handler;
+ (void)unregiserHandler;

/// MMKVLogInfo by default
/// MMKVLogNone to disable all logging
+ (void)setLogLevel:(MMKVLogLevel)logLevel __attribute__((deprecated("use +initializeMMKV:logLevel: instead", "initializeMMKV:nil logLevel")));

/// Migrate NSUserDefault data to MMKV
/// @param userDaults the NSUserDefaults instance to be imported
/// @return imported count of key-values
- (uint32_t)migrateFromUserDefaults:(NSUserDefaults *)userDaults NS_SWIFT_NAME(migrateFrom(userDefaults:));

/// detect if the MMKV file is valid or not
/// Note: Don't use this to check the existence of the instance, the return value is undefined if the file was never created.
+ (BOOL)isFileValid:(NSString *)mmapID NS_SWIFT_NAME(isFileValid(for:));
+ (BOOL)isFileValid:(NSString *)mmapID rootPath:(nullable NSString *)path NS_SWIFT_NAME(isFileValid(for:rootPath:));

NS_ASSUME_NONNULL_END

@end
