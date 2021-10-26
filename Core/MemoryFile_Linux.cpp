/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2021 THL A29 Limited, a Tencent company.
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

#if defined(MMKV_ANDROID) || defined(MMKV_LINUX)
#    include "InterProcessLock.h"
#    include "MMBuffer.h"
#    include "MMKVLog.h"
#    include "ScopedLock.hpp"
#    include <cerrno>
#    include <utility>
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#    include <sys/file.h>
#    include <dirent.h>
#    include <cstring>
#    include <sys/sendfile.h>
#    include <sys/syscall.h>

#ifdef MMKV_ANDROID
#include <dlfcn.h>
typedef int (*renameat2_t)(int old_dir_fd, const char* old_path, int new_dir_fd, const char* new_path, unsigned flags);
#endif

#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1) /* Exchange source and dest */
#endif

namespace mmkv {

extern bool getFileSize(int fd, size_t &size);

bool tryAtomicRename(const MMKVPath_t &srcPath, const MMKVPath_t &dstPath) {
    bool renamed = false;

    // try renameat2() first
#ifdef SYS_renameat2
#ifdef MMKV_ANDROID
    static auto g_renameat2 = (renameat2_t) dlsym(RTLD_DEFAULT, "renameat2");
    if (g_renameat2) {
        renamed = (g_renameat2(AT_FDCWD, srcPath.c_str(), AT_FDCWD, dstPath.c_str(), RENAME_EXCHANGE) == 0);
    }
#endif
    if (!renamed) {
        renamed = (syscall(SYS_renameat2, AT_FDCWD, srcPath.c_str(), AT_FDCWD, dstPath.c_str(), RENAME_EXCHANGE) == 0);
    }
    if (!renamed && errno != ENOENT) {
        MMKVError("fail on renameat2() [%s] to [%s], %d(%s)", srcPath.c_str(), dstPath.c_str(), errno, strerror(errno));
    }
#endif // SYS_renameat2

    if (!renamed) {
        if (::rename(srcPath.c_str(), dstPath.c_str()) != 0) {
            MMKVError("fail to rename [%s] to [%s], %d(%s)", srcPath.c_str(), dstPath.c_str(), errno, strerror(errno));
            return false;
        }
    }

    ::unlink(srcPath.c_str());
    return true;
}

// do it by sendfile()
bool copyFileContent(const MMKVPath_t &srcPath, MMKVFileHandle_t dstFD, bool needTruncate) {
    if (dstFD < 0) {
        return false;
    }
    File srcFile(srcPath, OpenFlag::ReadOnly);
    if (!srcFile.isFileValid()) {
        return false;
    }
    auto srcFileSize = srcFile.getActualFileSize();

    lseek(dstFD, 0, SEEK_SET);
    auto writtenSize = ::sendfile(dstFD, srcFile.getFd(), nullptr, srcFileSize);
    auto ret = (writtenSize == srcFileSize);
    if (!ret) {
        if (writtenSize < 0) {
            MMKVError("fail to sendfile() %s to fd[%d], %d(%s)", srcPath.c_str(), dstFD, errno, strerror(errno));
        } else {
            MMKVError("sendfile() %s to fd[%d], written %lld < %zu", srcPath.c_str(), dstFD, writtenSize, srcFileSize);
        }
    } else if (needTruncate) {
        size_t dstFileSize = 0;
        getFileSize(dstFD, dstFileSize);
        if ((dstFileSize != srcFileSize) && (::ftruncate(dstFD, static_cast<off_t>(srcFileSize)) != 0)) {
            MMKVError("fail to truncate [%d] to size [%zu], %d(%s)", dstFD, srcFileSize, errno, strerror(errno));
            ret = false;
        }
    }

    if (ret) {
        MMKVInfo("copy content from %s to fd[%d] finish", srcPath.c_str(), dstFD);
    }
    return ret;
}

} // namespace mmkv

#endif // defined(MMKV_ANDROID) || defined(MMKV_LINUX)