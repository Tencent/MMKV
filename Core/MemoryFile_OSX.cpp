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

#include "MemoryFile.h"
#include "MMKVLog.h"

#ifdef MMKV_IOS

using namespace std;

namespace mmkv {

void tryResetFileProtection(const string &path) {
    @autoreleasepool {
        NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
        NSDictionary *attr = [[NSFileManager defaultManager] attributesOfItemAtPath:nsPath error:nullptr];
        NSString *protection = [attr valueForKey:NSFileProtectionKey];
        MMKVInfo("protection on [%@] is %@", nsPath, protection);
        if ([protection isEqualToString:NSFileProtectionCompleteUntilFirstUserAuthentication] == NO) {
            NSMutableDictionary *newAttr = [NSMutableDictionary dictionaryWithDictionary:attr];
            [newAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication forKey:NSFileProtectionKey];
            NSError *err = nil;
            [[NSFileManager defaultManager] setAttributes:newAttr ofItemAtPath:nsPath error:&err];
            if (err != nil) {
                MMKVError("fail to set attribute %@ on [%@]: %@", NSFileProtectionCompleteUntilFirstUserAuthentication,
                          nsPath, err);
            }
        }
    }
}

} // namespace mmkv

#endif // MMKV_IOS

#ifdef MMKV_APPLE

#include <copyfile.h>
#include <unistd.h>

namespace mmkv {

bool tryAtomicRename(const char *src, const char *dst) {
    bool renamed = false;

    // try atomic swap first
    if (@available(iOS 10.0, watchOS 3.0, macOS 10.12, *)) {
        // renameat2() equivalent
        if (renamex_np(src, dst, RENAME_SWAP) == 0) {
            renamed = true;
        } else if (errno != ENOENT) {
            MMKVError("fail to renamex_np %s to %s, %s", src, dst, strerror(errno));
        }
    }

    if (!renamed) {
        // try old style rename
        if (rename(src, dst) != 0) {
            MMKVError("fail to rename %s to %s, %s", src, dst, strerror(errno));
            return false;
        }
    }

    ::unlink(src);

    return true;
}

bool copyFile(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    // prepare a temp file for atomic rename, avoid data corruption of suddent crash
    NSString *uniqueFileName = [NSString stringWithFormat:@"mmkv_%zu", (size_t) NSDate.timeIntervalSinceReferenceDate];
    NSString *tmpFile = [NSTemporaryDirectory() stringByAppendingPathComponent:uniqueFileName];
    if (copyfile(srcPath.c_str(), tmpFile.UTF8String, nullptr, COPYFILE_UNLINK | COPYFILE_CLONE) != 0) {
        MMKVError("fail to copyfile [%s] to [%s], %s", srcPath.c_str(), tmpFile.UTF8String, strerror(errno));
        return false;
    }
    MMKVInfo("copyfile [%s] to [%s]", srcPath.c_str(), tmpFile.UTF8String);

    if (tryAtomicRename(tmpFile.UTF8String, dstPath.c_str())) {
        MMKVInfo("copyfile [%s] to [%s] finish.", srcPath.c_str(), dstPath.c_str());
        return true;
    }
    unlink(tmpFile.UTF8String);
    return false;
}

bool copyFileContent(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    File dstFile(dstPath, OpenFlag::WriteOnly | OpenFlag::Create | OpenFlag::Truncate);
    if (!dstFile.isFileValid()) {
        return false;
    }
    if (copyFileContent(srcPath, dstFile.getFd())) {
        MMKVInfo("copy content from %s to fd[%s] finish", srcPath.c_str(), dstPath.c_str());
        return true;
    }
    MMKVError("fail to copyfile(): target file %s", dstPath.c_str());
    return false;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD) {
    if (dstFD < 0) {
        return false;
    }

    File srcFile(srcPath, OpenFlag::ReadOnly);
    if (!srcFile.isFileValid()) {
        return false;
    }

    // sendfile() equivalent
    if (::fcopyfile(srcFile.getFd(), dstFD, nullptr, COPYFILE_ALL) == 0) {
        MMKVInfo("copy content from %s to fd[%d] finish", srcPath.c_str(), dstFD);
        return true;
    }
    MMKVError("fail to copyfile(): %d(%s), source file %s", errno, strerror(errno), srcPath.c_str());
    return false;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD, bool needTruncate) {
    return copyFileContent(srcPath, dstFD);
}

} // namespace mmkv

#endif // MMKV_APPLE
