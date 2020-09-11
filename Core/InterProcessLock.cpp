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

bool FileLock::lock(LockType lockType) {
    return doLock(lockType, true);
}

bool FileLock::try_lock(LockType lockType, bool *tryAgain) {
    return doLock(lockType, false, tryAgain);
}

bool FileLock::doLock(LockType lockType, bool wait, bool *tryAgain) {
    if (!isFileLockValid()) {
        return false;
    }
    bool unLockFirstIfNeeded = false;

    if (lockType == SharedLockType) {
        // don't want shared-lock to break any existing locks
        if (m_sharedLockCount > 0 || m_exclusiveLockCount > 0) {
            m_sharedLockCount++;
            return true;
        }
    } else {
        // don't want exclusive-lock to break existing exclusive-locks
        if (m_exclusiveLockCount > 0) {
            m_exclusiveLockCount++;
            return true;
        }
        // prevent deadlock
        if (m_sharedLockCount > 0) {
            unLockFirstIfNeeded = true;
        }
    }

    auto ret = platformLock(lockType, wait, unLockFirstIfNeeded, tryAgain);
    if (ret) {
        if (lockType == SharedLockType) {
            m_sharedLockCount++;
        } else {
            m_exclusiveLockCount++;
        }
    }
    return ret;
}

#ifndef MMKV_WIN32

static int32_t LockType2FlockType(LockType lockType) {
    switch (lockType) {
        case SharedLockType:
            return LOCK_SH;
        case ExclusiveLockType:
            return LOCK_EX;
    }
    return LOCK_EX;
}

bool FileLock::platformLock(LockType lockType, bool wait, bool unLockFirstIfNeeded, bool *tryAgain) {
#    ifdef MMKV_ANDROID
    if (m_isAshmem) {
        return ashmemLock(lockType, wait, unLockFirstIfNeeded, tryAgain);
    }
#    endif
    auto realLockType = LockType2FlockType(lockType);
    auto cmd = wait ? realLockType : (realLockType | LOCK_NB);
    if (unLockFirstIfNeeded) {
        // try lock
        auto ret = flock(m_fd, realLockType | LOCK_NB);
        if (ret == 0) {
            return true;
        }
        // let's be gentleman: unlock my shared-lock to prevent deadlock
        ret = flock(m_fd, LOCK_UN);
        if (ret != 0) {
            MMKVError("fail to try unlock first fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        }
    }

    auto ret = flock(m_fd, cmd);
    if (ret != 0) {
        if (tryAgain) {
            *tryAgain = (errno == EWOULDBLOCK);
        }
        if (wait) {
            MMKVError("fail to lock fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        }
        // try recover my shared-lock
        if (unLockFirstIfNeeded) {
            ret = flock(m_fd, LockType2FlockType(SharedLockType));
            if (ret != 0) {
                // let's hope this never happen
                MMKVError("fail to recover shared-lock fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
            }
        }
        return false;
    } else {
        return true;
    }
}

bool FileLock::platformUnLock(bool unlockToSharedLock) {
#    ifdef MMKV_ANDROID
    if (m_isAshmem) {
        return ashmemUnLock(unlockToSharedLock);
    }
#    endif
    int cmd = unlockToSharedLock ? LOCK_SH : LOCK_UN;
    auto ret = flock(m_fd, cmd);
    if (ret != 0) {
        MMKVError("fail to unlock fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        return false;
    } else {
        return true;
    }
}

#endif // MMKV_WIN32

bool FileLock::unlock(LockType lockType) {
    if (!isFileLockValid()) {
        return false;
    }
    bool unlockToSharedLock = false;

    if (lockType == SharedLockType) {
        if (m_sharedLockCount == 0) {
            return false;
        }
        // don't want shared-lock to break any existing locks
        if (m_sharedLockCount > 1 || m_exclusiveLockCount > 0) {
            m_sharedLockCount--;
            return true;
        }
    } else {
        if (m_exclusiveLockCount == 0) {
            return false;
        }
        if (m_exclusiveLockCount > 1) {
            m_exclusiveLockCount--;
            return true;
        }
        // restore shared-lock when all exclusive-locks are done
        if (m_sharedLockCount > 0) {
            unlockToSharedLock = true;
        }
    }

    auto ret = platformUnLock(unlockToSharedLock);
    if (ret) {
        if (lockType == SharedLockType) {
            m_sharedLockCount--;
        } else {
            m_exclusiveLockCount--;
        }
    }
    return ret;
}

} // namespace mmkv
