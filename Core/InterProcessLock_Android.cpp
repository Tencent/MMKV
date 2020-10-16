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

#ifdef MMKV_ANDROID
#    include "MMKVLog.h"
#    include <sys/file.h>
#    include <unistd.h>

namespace mmkv {

FileLock::FileLock(MMKVFileHandle_t fd, bool isAshmem)
    : m_fd(fd), m_sharedLockCount(0), m_exclusiveLockCount(0), m_isAshmem(isAshmem) {
    m_lockInfo.l_type = F_WRLCK;
    m_lockInfo.l_start = 0;
    m_lockInfo.l_whence = SEEK_SET;
    m_lockInfo.l_len = 0;
    m_lockInfo.l_pid = 0;
}

static short LockType2FlockType(LockType lockType) {
    switch (lockType) {
        case SharedLockType:
            return F_RDLCK;
        case ExclusiveLockType:
            return F_WRLCK;
    }
}

bool FileLock::ashmemLock(LockType lockType, bool wait, bool unLockFirstIfNeeded, bool *tryAgain) {
    m_lockInfo.l_type = LockType2FlockType(lockType);
    if (unLockFirstIfNeeded) {
        // try lock
        auto ret = fcntl(m_fd, F_SETLK, &m_lockInfo);
        if (ret == 0) {
            return true;
        }
        // lets be gentleman: unlock my shared-lock to prevent deadlock
        auto type = m_lockInfo.l_type;
        m_lockInfo.l_type = F_UNLCK;
        ret = fcntl(m_fd, F_SETLK, &m_lockInfo);
        if (ret != 0) {
            MMKVError("fail to try unlock first fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        }
        m_lockInfo.l_type = type;
    }

    int cmd = wait ? F_SETLKW : F_SETLK;
    auto ret = fcntl(m_fd, cmd, &m_lockInfo);
    if (ret != 0) {
        if (tryAgain) {
            *tryAgain = (errno == EAGAIN);
        }
        if (wait) {
            MMKVError("fail to lock fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        }
        // try recover my shared-lock
        if (unLockFirstIfNeeded) {
            m_lockInfo.l_type = LockType2FlockType(SharedLockType);
            ret = fcntl(m_fd, cmd, &m_lockInfo);
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

bool FileLock::ashmemUnLock(bool unlockToSharedLock) {
    m_lockInfo.l_type = static_cast<short>(unlockToSharedLock ? F_RDLCK : F_UNLCK);
    auto ret = fcntl(m_fd, F_SETLK, &m_lockInfo);
    if (ret != 0) {
        MMKVError("fail to unlock fd=%d, ret=%d, error:%s", m_fd, ret, strerror(errno));
        return false;
    } else {
        return true;
    }
}

} // namespace mmkv

#endif // MMKV_ANDROID
