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

namespace mmkv {

enum LockType {
    SharedLockType,
    ExclusiveLockType,
};

// a recursive Win32 file-lock wrapper
// handles lock upgrade & downgrade correctly
class FileLock {
    HANDLE m_fd;
    OVERLAPPED m_overLapped;
    size_t m_sharedLockCount;
    size_t m_exclusiveLockCount;

    // just forbid it for possibly misuse
    FileLock(const FileLock &other) = delete;

    FileLock &operator=(const FileLock &other) = delete;

public:
    FileLock(HANDLE fd);

    bool lock(LockType lockType);

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

    void unlock() {
        if (m_enable) {
            m_fileLock->unlock(m_lockType);
        }
    }
};

} // namespace mmkv

#endif //MMKV_INTERPROCESSLOCK_H
