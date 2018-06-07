//
// Created by Ling Guo on 2018/5/28.
//

#include "ThreadLock.h"
#include "MMKVLog.h"

/*
#include <time.h>
#if __ANDROID_API__ >= 21
    timespec absTime2RealTime(size_t milliseconds) {
        timespec future;
        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        future.tv_sec = now.tv_sec + milliseconds/1000;
        future.tv_nsec = (now.tv_sec * 1000) + ((milliseconds%1000) * 1000000);
        if (future.tv_nsec >= 1000000000) {
            future.tv_sec++;
            future.tv_nsec -= 1000000000;
        }
        return future;
    }
                timespec timeoutTime = absTime2RealTime(msToWait);
                auto ret = pthread_mutex_timedlock(m_oLock, &timeoutTime);
                if (ret != 0 && ret != ETIMEDOUT) {
                    MMKVError("fail to lock %p, ret=%d, errno=%s", m_oLock, ret, strerror(errno));
                }
                m_isLocked = (ret == 0);
                m_isProcessLock = true;
                if (m_isLocked) {
                    MMKVInfo("locked %p", m_oLock);
                }
#endif
*/

ThreadLock::ThreadLock() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&m_lock, &attr);

    pthread_mutexattr_destroy(&attr);
}

ThreadLock::~ThreadLock() {
    pthread_mutex_destroy(&m_lock);
}

void ThreadLock::lock() {
    auto ret = pthread_mutex_lock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to lock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

bool ThreadLock::try_lock() {
    auto ret = pthread_mutex_trylock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to try lock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
    return (ret == 0);
}

void ThreadLock::unlock() {
    auto ret = pthread_mutex_unlock(&m_lock);
    if (ret != 0) {
        MMKVError("fail to unlock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}
