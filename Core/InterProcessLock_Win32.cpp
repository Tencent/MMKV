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

#include "InterProcessLock.h"

#ifdef MMKV_WIN32
#    include "MMKVLog.h"

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

bool FileLock::platformLock(LockType lockType, bool wait, bool unLockFirstIfNeeded, bool *tryAgain) {
    auto realLockType = LockType2Flag(lockType);
    auto flag = wait ? realLockType : (realLockType | LOCKFILE_FAIL_IMMEDIATELY);
    if (unLockFirstIfNeeded) {
        /* try exclusive-lock above shared-lock will always fail in Win32
		auto ret = LockFileEx(m_fd, realLockType | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &m_overLapped);
		if (ret) {
			return true;
		}*/
        // let's be gentleman: unlock my shared-lock to prevent deadlock
        auto ret = UnlockFileEx(m_fd, 0, 1, 0, &m_overLapped);
        if (!ret) {
            MMKVError("fail to try unlock first fd=%p, error:%d", m_fd, GetLastError());
        }
    }

    auto ret = LockFileEx(m_fd, flag, 0, 1, 0, &m_overLapped);
    if (!ret) {
        if (tryAgain) {
            *tryAgain = (GetLastError() == ERROR_LOCK_VIOLATION);
        }
        if (wait) {
            MMKVError("fail to lock fd=%p, error:%d", m_fd, GetLastError());
        }
        // try recover my shared-lock
        if (unLockFirstIfNeeded) {
            ret = LockFileEx(m_fd, LockType2Flag(SharedLockType), 0, 1, 0, &m_overLapped);
            if (!ret) {
                // let's hope this never happen
                MMKVError("fail to recover shared-lock fd=%p, error:%d", m_fd, GetLastError());
            }
        }
        return false;
    } else {
        return true;
    }
}

bool FileLock::platformUnLock(bool unlockToSharedLock) {
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

#endif // MMKV_WIN32
