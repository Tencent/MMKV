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
#include <mach/mach_time.h>

namespace mmkv {

static bool atomicRename(const char *src, const char *dst) {
    bool renamed = false;

    // try atomic swap first
    if (@available(iOS 10.0, watchOS 3.0, *)) {
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

    unlink(src);

    return true;
}

bool copyFile(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    // prepare a temp file for atomic rename, avoid data corruption of suddent crash
    NSString *uniqueFileName = [NSString stringWithFormat:@"mmkv_%llu", mach_absolute_time()];
    NSString *tmpFile = [NSTemporaryDirectory() stringByAppendingPathComponent:uniqueFileName];
    if (copyfile(srcPath.c_str(), tmpFile.UTF8String, nullptr, COPYFILE_UNLINK | COPYFILE_CLONE) != 0) {
        MMKVError("fail to copyfile [%s] to [%s], %s", srcPath.c_str(), tmpFile.UTF8String, strerror(errno));
        return false;
    }
    MMKVInfo("copyfile [%s] to [%s]", srcPath.c_str(), tmpFile.UTF8String);

    if (atomicRename(tmpFile.UTF8String, dstPath.c_str())) {
        MMKVInfo("copyfile [%s] to [%s] finish.", srcPath.c_str(), dstPath.c_str());
        return true;
    }
    unlink(tmpFile.UTF8String);
    return false;
}

bool copyFileContent(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    auto dstFD = open(dstPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU);
    if (dstFD < 0) {
        MMKVError("fail to open:%s, %s", dstPath.c_str(), strerror(errno));
        return false;
    }
    auto ret = copyFileContent(srcPath, dstFD);
    if (!ret) {
        MMKVError("fail to copyfile(): target file %s", dstPath.c_str());
    } else {
        MMKVInfo("copy content from %s to fd[%s] finish", srcPath.c_str(), dstPath.c_str());
    }
    ::close(dstFD);

    return ret;
}

bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD) {
    if (dstFD < 0) {
        return false;
    }

    auto srcFD = open(srcPath.c_str(), O_RDONLY | O_CLOEXEC, S_IRWXU);
    if (srcFD < 0) {
        MMKVError("fail to open:%s, %s", srcPath.c_str(), strerror(errno));
        return false;
    }

    // sendfile() equivalent
    auto ret = (::fcopyfile(srcFD, dstFD, nullptr, COPYFILE_DATA) == 0);
    if (!ret) {
        MMKVError("fail to copyfile(): %d(%s), source file %s", errno, strerror(errno), srcPath.c_str());
    }
    ::close(srcFD);
    MMKVInfo("copy content from %s to fd[%d] finish", srcPath.c_str(), dstFD);

    return ret;
}

} // namespace mmkv

#endif // MMKV_APPLE
