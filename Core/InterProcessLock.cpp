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

#include "InterProcessLock.h"
#include "MMKVLog.h"

#ifndef MMKV_WIN32
#    include <sys/file.h>
#endif

namespace mmkv {
#ifndef MMKV_WIN32
static uint32_t LockType2FlockType(LockType lockType) {
    switch (lockType) {
        case SharedLockType:
            return LOCK_SH;
        case ExclusiveLockType:
            return LOCK_EX;
    }
    return LOCK_EX;
}
#else
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
#endif

bool FileLock::doLock(LockType lockType, bool wait) {
    if (!isFileLockValid()) {
        return false;
    }
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

#ifndef MMKV_WIN32
    auto realLockType = LockType2FlockType(lockType);
    int cmd = wait ? realLockType : (realLockType | LOCK_NB);
    if (unLockFirstIfNeeded) {
        // try lock
        auto ret = flock(m_fd, realLockType | LOCK_NB);
        if (ret == 0) {
            return true;
        }
        // lets be gentleman: unlock my shared-lock to prevent deadlock
        ret = flock(m_fd, LOCK_UN);
        if (ret != 0) {
            MMKVError("fail to try unlock first fd=%d, ret=%d, error:%s", m_fd, ret,
                      strerror(errno));
        }
    }

    auto ret = flock(m_fd, cmd);
    if (ret != 0) {
        MMKVError("fail to lock fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        return false;
    } else {
        return true;
    }
#else
    auto realLockType = LockType2Flag(lockType);
    auto flag = wait ? realLockType : (realLockType | LOCKFILE_FAIL_IMMEDIATELY);
    if (unLockFirstIfNeeded) {
        /* try exclusive-lock above shared-lock will always fail in Win32
		auto ret = LockFileEx(m_fd, realLockType | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &m_overLapped);
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
#endif
}

bool FileLock::lock(LockType lockType) {
    return doLock(lockType, true);
}

bool FileLock::try_lock(LockType lockType) {
    return doLock(lockType, false);
}

bool FileLock::unlock(LockType lockType) {
    if (!isFileLockValid()) {
        return false;
    }
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

#ifndef MMKV_WIN32
    int cmd = unlockToSharedLock ? LOCK_SH : LOCK_UN;
    auto ret = flock(m_fd, cmd);
    if (ret != 0) {
        MMKVError("fail to unlock fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        return false;
    } else {
        return true;
    }
#else
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
#endif
}

} // namespace mmkv
