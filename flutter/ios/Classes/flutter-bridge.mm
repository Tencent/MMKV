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

#import <MMKV/MMKV.h>
#import <stdint.h>
#import <string>

#define MMKV_EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))
#define MMKV_FUNC(func) mmkv_ ## func

using namespace std;

MMKV_EXPORT void *getMMKVWithID(const char *mmapID, uint32_t mode, const char *cryptKey, const char *rootPath) {
    MMKV *kv = nil;
    if (!mmapID) {
        return (__bridge void *) kv;
    }
    NSString *str = [NSString stringWithUTF8String:mmapID];

    bool done = false;
    if (cryptKey) {
        auto crypt = [NSString stringWithUTF8String:cryptKey];
        if (crypt.length > 0) {
            auto cryptKeyData = [crypt dataUsingEncoding:NSUTF8StringEncoding];
            if (rootPath) {
                auto path = [NSString stringWithUTF8String:rootPath];
                kv = [MMKV mmkvWithID:str cryptKey:cryptKeyData rootPath:path];
            } else {
                kv = [MMKV mmkvWithID:str cryptKey:cryptKeyData mode:(MMKVMode) mode];
            }
            done = true;
        }
    }
    if (!done) {
        if (rootPath) {
            auto path = [NSString stringWithUTF8String:rootPath];
            kv = [MMKV mmkvWithID:str rootPath:path];
        } else {
            kv = [MMKV mmkvWithID:str mode:(MMKVMode) mode];
        }
    }

    return (__bridge void *) kv;
}

MMKV_EXPORT int64_t getDefaultMMKV(int /*mode*/, const char *cryptKey) {
    MMKV *kv = nil;

    if (cryptKey) {
        auto crypt = [NSString stringWithUTF8String:cryptKey];
        auto cryptKeyData = [crypt dataUsingEncoding:NSUTF8StringEncoding];
        if (cryptKeyData.length > 0) {
            kv = [MMKV defaultMMKVWithCryptKey:cryptKeyData];
        }
    }
    if (!kv) {
        kv = [MMKV defaultMMKV];
    }

    return (int64_t) kv;
}

MMKV_EXPORT const char *MMKV_FUNC(mmapID)(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        return [[kv mmapID] UTF8String];
    }
    return nullptr;
}

MMKV_EXPORT bool MMKV_FUNC(encodeBool)(const void *handle, const char *oKey, bool value) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv setBool:value forKey:key];
    }
    return false;
}

MMKV_EXPORT bool MMKV_FUNC(decodeBool)(const void *handle, const char *oKey, bool defaultValue) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv getBoolForKey:key defaultValue:defaultValue];
    }
    return defaultValue;
}

MMKV_EXPORT bool MMKV_FUNC(encodeInt32)(const void *handle, const char *oKey, int32_t value) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv setInt32:value forKey:key];
    }
    return false;
}

MMKV_EXPORT int32_t MMKV_FUNC(decodeInt32)(const void *handle, const char *oKey, int32_t defaultValue) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv getInt32ForKey:key defaultValue:defaultValue];
    }
    return defaultValue;
}

MMKV_EXPORT bool MMKV_FUNC(encodeInt64)(const void *handle, const char *oKey, int64_t value) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv setInt64:value forKey:key];
    }
    return false;
}

MMKV_EXPORT int64_t MMKV_FUNC(decodeInt64)(const void *handle, const char *oKey, int64_t defaultValue) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv getInt64ForKey:key defaultValue:defaultValue];
    }
    return defaultValue;
}

MMKV_EXPORT bool MMKV_FUNC(encodeDouble)(const void *handle, const char *oKey, double value) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv setDouble:value forKey:key];
    }
    return false;
}

MMKV_EXPORT double MMKV_FUNC(decodeDouble)(const void *handle, const char *oKey, double defaultValue) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv getDoubleForKey:key defaultValue:defaultValue];
    }
    return defaultValue;
}

MMKV_EXPORT bool MMKV_FUNC(encodeBytes)(const void *handle, const char *oKey, void *oValue, uint64_t length) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        if (oValue) {
            auto value = [NSData dataWithBytesNoCopy:oValue length:static_cast<NSUInteger>(length) freeWhenDone:NO];
            return [kv setData:value forKey:key];
        } else {
            [kv removeValueForKey:key];
            return true;
        }
    }
    return false;
}

MMKV_EXPORT void *MMKV_FUNC(decodeBytes)(const void *handle, const char *oKey, uint64_t *lengthPtr) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        auto value = [kv getDataForKey:key];
        if (value) {
            *lengthPtr = value.length;
            if (value.bytes) {
                return (void *) value.bytes;
            } else {
                return (__bridge void *) value;
            }
        }
    }
    return nullptr;
}

MMKV_EXPORT bool MMKV_FUNC(reKey)(const void *handle, char *oKey, uint64_t length) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        if (oKey && length > 0) {
            NSData *key = [NSData dataWithBytesNoCopy:oKey length:length freeWhenDone:NO];
            return [kv reKey:key];
        } else {
            return [kv reKey:nil];
        }
    }
    return false;
}

MMKV_EXPORT void *MMKV_FUNC(cryptKey)(const void *handle, uint64_t *lengthPtr) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && lengthPtr) {
        auto cryptKey = [kv cryptKey];
        if (cryptKey.length > 0) {
            auto ptr = malloc(cryptKey.length);
            if (ptr) {
                memcpy(ptr, cryptKey.bytes, cryptKey.length);
                *lengthPtr = cryptKey.length;
                return ptr;
            }
        }
    }
    return nullptr;
}

MMKV_EXPORT void MMKV_FUNC(checkReSetCryptKey)(const void *handle, char *oKey, uint64_t length) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        if (oKey && length > 0) {
            NSData *key = [NSData dataWithBytesNoCopy:oKey length:length freeWhenDone:NO];
            [kv checkReSetCryptKey:key];
        } else {
            [kv checkReSetCryptKey:nil];
        }
    }
}

MMKV_EXPORT uint32_t MMKV_FUNC(valueSize)(const void *handle, char *oKey, bool actualSize) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        auto ret = [kv getValueSizeForKey:key actualSize:actualSize];
        return static_cast<uint32_t>(ret);
    }
    return 0;
}

MMKV_EXPORT int32_t MMKV_FUNC(writeValueToNB)(const void *handle, char *oKey, void *pointer, uint32_t size) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv writeValueForKey:key toBuffer:[NSMutableData dataWithBytesNoCopy:pointer length:size freeWhenDone:NO]];
    }
    return -1;
}

MMKV_EXPORT uint64_t MMKV_FUNC(allKeys)(const void *handle, char ***keyArrayPtr, uint32_t **sizeArrayPtr) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        auto keys = [kv allKeys];
        if (keys.count > 0) {
            auto keyArray = (char **) malloc(keys.count * sizeof(void *));
            auto sizeArray = (uint32_t *) malloc(keys.count * sizeof(uint32_t *));
            if (!keyArray || !sizeArray) {
                free(keyArray);
                free(sizeArray);
                return 0;
            }
            *keyArrayPtr = keyArray;
            *sizeArrayPtr = sizeArray;

            for (size_t index = 0; index < keys.count; index++) {
                NSString *key = keys[index];
                auto keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
                sizeArray[index] = static_cast<uint32_t>(keyData.length);
                keyArray[index] = (char *) keyData.bytes;
            }
        }
        return keys.count;
    }
    return 0;
}

MMKV_EXPORT bool MMKV_FUNC(containsKey)(const void *handle, char *oKey) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        return [kv containsKey:key];
    }
    return false;
}

MMKV_EXPORT uint64_t MMKV_FUNC(count)(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        return [kv count];
    }
    return 0;
}

MMKV_EXPORT uint64_t MMKV_FUNC(totalSize)(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        return [kv totalSize];
    }
    return 0;
}

MMKV_EXPORT uint64_t MMKV_FUNC(actualSize)(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        return [kv actualSize];
    }
    return 0;
}

MMKV_EXPORT void MMKV_FUNC(removeValueForKey)(const void *handle, char *oKey) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && oKey) {
        auto key = [NSString stringWithUTF8String:oKey];
        [kv removeValueForKey:key];
    }
}

MMKV_EXPORT void MMKV_FUNC(removeValuesForKeys)(const void *handle, char **keyArray, uint32_t *sizeArray, uint64_t count) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv && keyArray && sizeArray && count > 0) {
        NSMutableArray *arrKeys = [NSMutableArray arrayWithCapacity:count];
        for (uint64_t index = 0; index < count; index++) {
            if (sizeArray[index] > 0 && keyArray[index]) {
                auto keyData = [NSData dataWithBytesNoCopy:keyArray[index] length:sizeArray[index] freeWhenDone:NO];
                NSString *key = [[NSString alloc] initWithData:keyData encoding:NSUTF8StringEncoding];
                if (key.length > 0) {
                    [arrKeys addObject:key];
                }
            }
        }
        if (arrKeys.count > 0) {
            [kv removeValuesForKeys:arrKeys];
        }
    }
}

MMKV_EXPORT void MMKV_FUNC(clearAll)(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        [kv clearAll];
    }
}

MMKV_EXPORT void mmkvSync(const void *handle, bool sync) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        if (sync) {
            [kv sync];
        } else {
            [kv async];
        }
    }
}

MMKV_EXPORT void MMKV_FUNC(clearMemoryCache)(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        [kv clearMemoryCache];
    }
}

MMKV_EXPORT int32_t MMKV_FUNC(pageSize)() {
    return static_cast<int32_t>([MMKV pageSize]);
}

MMKV_EXPORT const char *MMKV_FUNC(version)() {
    return [MMKV version].UTF8String;
}

MMKV_EXPORT void MMKV_FUNC(trim)(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        [kv trim];
    }
}

MMKV_EXPORT void mmkvClose(const void *handle) {
    MMKV *kv = (__bridge MMKV *) handle;
    if (kv) {
        [kv close];
    }
}

MMKV_EXPORT void mmkvMemcpy(void *dst, const void *src, uint64_t size) {
    memcpy(dst, src, size);
}

MMKV_EXPORT bool MMKV_FUNC(backupOne)(const char *mmapID, const char *dstDir, const char *rootPath) {
    auto strID = [NSString stringWithUTF8String:mmapID];
    auto strDstDir = [NSString stringWithUTF8String:dstDir];

    if (rootPath) {
        auto root = [NSString stringWithUTF8String:rootPath];
        if (root.length > 0) {
            return [MMKV backupOneMMKV:strID rootPath:root toDirectory:strDstDir];
        }
    }
    return [MMKV backupOneMMKV:strID rootPath:nil toDirectory:strDstDir];
}

MMKV_EXPORT bool MMKV_FUNC(restoreOne)(const char *mmapID, const char *srcDir, const char *rootPath) {
    auto strID = [NSString stringWithUTF8String:mmapID];
    auto strSrcDir = [NSString stringWithUTF8String:srcDir];
    if (rootPath) {
        auto root = [NSString stringWithUTF8String:rootPath];
        if (root.length > 0) {
            return [MMKV restoreOneMMKV:strID rootPath:root fromDirectory:strSrcDir];
        }
    }
    return [MMKV restoreOneMMKV:strID rootPath:nil fromDirectory:strSrcDir];
}

MMKV_EXPORT uint64_t MMKV_FUNC(backupAll)(const char *dstDir/*, const char *rootPath*/) {
    auto strDstDir = [NSString stringWithUTF8String:dstDir];
    return [MMKV backupAll:nil toDirectory:strDstDir];
}

MMKV_EXPORT uint64_t MMKV_FUNC(restoreAll)(const char *srcDir/*, const char *rootPath*/) {
    auto strSrcDir = [NSString stringWithUTF8String:srcDir];
    return [MMKV restoreAll:nil fromDirectory:strSrcDir];
}

/* Looks like Dart:ffi's async callback not working perfectly
 * We don't support them for the moment.
 * https://github.com/dart-lang/sdk/issues/37022

#pragma mark - callback

using ErrorCallback_t = uint32_t (*)(const char *mmapID);
using ContenctChangeCallback_t = void (*)(const char *mmapID);
using LogCallback_t = void (*)(uint32_t level, const char *file, int32_t line, const char *funcname, const char *message);

@interface MyMMKVHandler : NSObject<MMKVHandler>

@property (atomic, assign) ErrorCallback_t lengthErrorCallback;
@property (atomic, assign) ErrorCallback_t crcErrorCallback;
@property (atomic, assign) LogCallback_t logCallback;
@property (atomic, assign) ContenctChangeCallback_t contenctChangeCallback;

@end

@implementation MyMMKVHandler

- (MMKVRecoverStrategic)onMMKVCRCCheckFail:(NSString *)mmapID {
    if (self.crcErrorCallback) {
        return (MMKVRecoverStrategic)self.crcErrorCallback(mmapID.UTF8String);
    }
    return MMKVOnErrorDiscard;
}

// by default MMKV will discard all datas on file length mismatch
// return `MMKVOnErrorRecover` to recover any data on the file
- (MMKVRecoverStrategic)onMMKVFileLengthError:(NSString *)mmapID {
    if (self.lengthErrorCallback) {
        return (MMKVRecoverStrategic)self.lengthErrorCallback(mmapID.UTF8String);
    }
    return MMKVOnErrorDiscard;
}

// by default MMKV will print log using NSLog
// implement this method to redirect MMKV's log
- (void)mmkvLogWithLevel:(MMKVLogLevel)level file:(const char *)file line:(int)line func:(const char *)funcname message:(NSString *)message {
    if (self.logCallback) {
        self.logCallback((uint32_t)level, file, line, funcname, message.UTF8String);
    } else {
        auto LogLevelDesc = [](MMKVLogLevel level) {
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
        };
        NSLog(@"mmkv:[%s] <%s:%d::%s> %@\n", LogLevelDesc(level), file, line, funcname, message);
    }
}

- (void)onMMKVContentChange:(NSString *)mmapID {
    if (self.contenctChangeCallback) {
        self.contenctChangeCallback(mmapID.UTF8String);
    }
}

@end

MyMMKVHandler *getHandler() {
    static MyMMKVHandler *g_mmkvHandler = nullptr;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        g_mmkvHandler = [[MyMMKVHandler alloc] init];
        [MMKV registerHandler:g_mmkvHandler];
    });
    return g_mmkvHandler;
}

MMKV_EXPORT void setWantsLogRedirect(LogCallback_t callback) {
    getHandler().logCallback = callback;
}

MMKV_EXPORT void setWantsHandleLengthError(ErrorCallback_t callback) {
    getHandler().lengthErrorCallback = callback;
}

MMKV_EXPORT void setWantsHandleCRCError(ErrorCallback_t callback) {
    getHandler().crcErrorCallback = callback;
}

MMKV_EXPORT void setWantsContentChangeNotify(ContenctChangeCallback_t callback) {
    getHandler().contenctChangeCallback = callback;
}

MMKV_EXPORT void checkContentChanged(const void *handle) {
    MMKV *kv = (__bridge MMKV*)handle;
    if (kv) {
        [kv checkContentChanged];
    }
}
*/

@interface MMKVDummy : NSObject
@end

@implementation MMKVDummy
@end
