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

#ifndef MMKV_INTERPROCESSLOCK_H
#define MMKV_INTERPROCESSLOCK_H
#ifdef __cplusplus

#    include "MMKVPredef.h"

#    include <fcntl.h>

namespace mmkv {

enum LockType {
    SharedLockType,
    ExclusiveLockType,
};

// a recursive POSIX file-lock wrapper
// handles lock upgrade & downgrade correctly
class FileLock {
    MMKVFileHandle_t m_fd;
    size_t m_sharedLockCount;
    size_t m_exclusiveLockCount;

    bool doLock(LockType lockType, bool wait, bool *tryAgain = nullptr);
    bool platformLock(LockType lockType, bool wait, bool unLockFirstIfNeeded, bool *tryAgain);
    bool platformUnLock(bool unLockFirstIfNeeded);

#    ifndef MMKV_WIN32
    bool isFileLockValid() { return m_fd >= 0; }
#        ifdef MMKV_ANDROID
    const bool m_isAshmem;
    struct flock m_lockInfo;
    bool ashmemLock(LockType lockType, bool wait, bool unLockFirstIfNeeded, bool *tryAgain);
    bool ashmemUnLock(bool unLockFirstIfNeeded);
#        endif

#    else  // defined(MMKV_WIN32)
    OVERLAPPED m_overLapped;

    bool isFileLockValid() { return m_fd != INVALID_HANDLE_VALUE; }
#    endif // MMKV_WIN32

public:
#    ifndef MMKV_WIN32
#        ifndef MMKV_ANDROID
    explicit FileLock(MMKVFileHandle_t fd) : m_fd(fd), m_sharedLockCount(0), m_exclusiveLockCount(0) {}
#        else
    explicit FileLock(MMKVFileHandle_t fd, bool isAshmem = false);
#        endif // MMKV_ANDROID
#    else      // defined(MMKV_WIN32)
    explicit FileLock(MMKVFileHandle_t fd) : m_fd(fd), m_overLapped{}, m_sharedLockCount(0), m_exclusiveLockCount(0) {}
#    endif     // MMKV_WIN32

    bool lock(LockType lockType);

    bool try_lock(LockType lockType, bool *tryAgain);

    bool unlock(LockType lockType);

    // just forbid it for possibly misuse
    explicit FileLock(const FileLock &other) = delete;
    FileLock &operator=(const FileLock &other) = delete;
};

class InterProcessLock {
    FileLock *m_fileLock;
    LockType m_lockType;

public:
    InterProcessLock(FileLock *fileLock, LockType lockType)
        : m_fileLock(fileLock), m_lockType(lockType), m_enable(true) {
        MMKV_ASSERT(m_fileLock);
    }

    bool m_enable;

    void lock() {
        if (m_enable) {
            m_fileLock->lock(m_lockType);
        }
    }

    bool try_lock(bool *tryAgain = nullptr) {
        if (m_enable) {
            return m_fileLock->try_lock(m_lockType, tryAgain);
        }
        return false;
    }

    void unlock() {
        if (m_enable) {
            m_fileLock->unlock(m_lockType);
        }
    }
};

} // namespace mmkv

#endif // __cplusplus
#endif // MMKV_INTERPROCESSLOCK_H
