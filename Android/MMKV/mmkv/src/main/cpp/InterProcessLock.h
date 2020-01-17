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

#include <cassert>
#include <fcntl.h>

enum LockType {
    SharedLockType,
    ExclusiveLockType,
};

// a recursive POSIX file-lock wrapper
// handles lock upgrade & downgrade correctly
class FileLock {
    int m_fd;
    size_t m_sharedLockCount;
    size_t m_exclusiveLockCount;

    bool doLock(LockType lockType, bool wait);
    bool platformLock(LockType lockType, bool wait, bool unLockFirstIfNeeded);
    bool platformUnLock(bool unLockFirstIfNeeded);

    bool isFileLockValid() { return m_fd >= 0; }

    const bool m_isAshmem;
    struct flock m_lockInfo;
    bool ashmemLock(LockType lockType, bool wait, bool unLockFirstIfNeeded);
    bool ashmemUnLock(bool unLockFirstIfNeeded);

    // just forbid it for possibly misuse
    FileLock(const FileLock &other) = delete;

    FileLock &operator=(const FileLock &other) = delete;

public:
    explicit FileLock(int fd, bool isAshmem = false);

    bool lock(LockType lockType);

    bool try_lock(LockType lockType);

    bool unlock(LockType lockType);
};

class InterProcessLock {
    FileLock *m_fileLock;
    LockType m_lockType;

public:
    InterProcessLock(FileLock *fileLock, LockType lockType)
        : m_fileLock(fileLock), m_lockType(lockType), m_enable(true) {
        assert(m_fileLock);
    }

    bool m_enable;

    void lock() {
        if (m_enable) {
            m_fileLock->lock(m_lockType);
        }
    }

    bool try_lock() {
        if (m_enable) {
            return m_fileLock->try_lock(m_lockType);
        }
        return false;
    }

    void unlock() {
        if (m_enable) {
            m_fileLock->unlock(m_lockType);
        }
    }
};

#endif //MMKV_INTERPROCESSLOCK_H
