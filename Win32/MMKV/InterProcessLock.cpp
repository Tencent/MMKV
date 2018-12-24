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

#include "pch.h"

#include "InterProcessLock.h"
#include "MMKVLog.h"

namespace mmkv {

static DWORD LockType2Flag(LockType lockType) {
    DWORD flag = 0;
    switch (lockType) {
        case SharedLockType:
            flag = 0;
            break;
        case ExclusiveLockType:
            flag = LOCKFILE_EXCLUSIVE_LOCK;
            break;
    }
    return flag;
}

FileLock::FileLock(HANDLE fd)
    : m_fd(fd), m_overLapped{0}, m_sharedLockCount(0), m_exclusiveLockCount(0) {}

bool FileLock::lock(LockType lockType) {
    bool unLockFirstIfNeeded = false;

    if (lockType == SharedLockType) {
        m_sharedLockCount++;
        // don't want shared-lock to break any existing locks
        if (m_sharedLockCount > 1 || m_exclusiveLockCount > 0) {
            return true;
        }
    } else {
        m_exclusiveLockCount++;
        // don't want exclusive-lock to break existing exclusive-locks
        if (m_exclusiveLockCount > 1) {
            return true;
        }
        // prevent deadlock
        if (m_sharedLockCount > 0) {
            unLockFirstIfNeeded = true;
        }
    }

    auto flag = LockType2Flag(lockType);
    if (unLockFirstIfNeeded) {
        /* try exclusive-lock above shared-lock will always fail in Win32
        auto ret = LockFileEx(m_fd, flag | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &m_overLapped);
        if (ret) {
            return true;
        }*/
        // lets be gentleman: unlock my shared-lock to prevent deadlock
        auto ret = UnlockFileEx(m_fd, 0, 1, 0, &m_overLapped);
        if (!ret) {
            MMKVError("fail to try unlock first fd=%p, error:%d", m_fd, GetLastError());
        }
    }

    auto ret = LockFileEx(m_fd, flag, 0, 1, 0, &m_overLapped);
    if (!ret) {
        MMKVError("fail to lock fd=%p, error:%d", m_fd, GetLastError());
        return false;
    } else {
        return true;
    }
}

bool FileLock::unlock(LockType lockType) {
    bool unlockToSharedLock = false;

    if (lockType == SharedLockType) {
        if (m_sharedLockCount == 0) {
            return false;
        }
        m_sharedLockCount--;
        // don't want shared-lock to break any existing locks
        if (m_sharedLockCount > 0 || m_exclusiveLockCount > 0) {
            return true;
        }
    } else {
        if (m_exclusiveLockCount == 0) {
            return false;
        }
        m_exclusiveLockCount--;
        if (m_exclusiveLockCount > 0) {
            return true;
        }
        // restore shared-lock when all exclusive-locks are done
        if (m_sharedLockCount > 0) {
            unlockToSharedLock = true;
        }
    }

    /* quote from MSDN:
     * If the same range is locked with an exclusive and a shared lock,
     * two unlock operations are necessary to unlock the region;
     * the first unlock operation unlocks the exclusive lock,
     * the second unlock operation unlocks the shared lock.
    */
    if (unlockToSharedLock) {
        auto flag = LockType2Flag(SharedLockType);
        if (!LockFileEx(m_fd, flag, 0, 1, 0, &m_overLapped)) {
            MMKVError("fail to roll back to shared-lock, error:%d", GetLastError());
        }
    }
    auto ret = UnlockFileEx(m_fd, 0, 1, 0, &m_overLapped);
    if (!ret) {
        MMKVError("fail to unlock fd=%p, error:%d", m_fd, GetLastError());
        return false;
    } else {
        return true;
    }
}

} // namespace mmkv
