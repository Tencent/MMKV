//
// Created by Ling Guo on 2018/4/25.
//

#ifndef MMKV_SCOPEDLOCK_HPP
#define MMKV_SCOPEDLOCK_HPP

#include <pthread.h>

class ScopedLock
{
    pthread_mutex_t* m_oLock;

public:
    ScopedLock(pthread_mutex_t* oLock) : m_oLock(oLock)
    {
        pthread_mutex_lock(m_oLock);
    }
    ~ScopedLock()
    {
        pthread_mutex_unlock(m_oLock);
        m_oLock = nullptr;
    }

    static pthread_mutex_t* GenRecursiveLock()
    {
        auto lock = new pthread_mutex_t;

        pthread_mutexattr_t Attr;
        pthread_mutexattr_init(&Attr);
        pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(lock, &Attr);

        return  lock;
    }
    static void DestroyRecursiveLock(pthread_mutex_t* lock)
    {
        if (lock) {
            pthread_mutex_destroy(lock);
            delete lock;
        }
    }
};


#endif //MMKV_SCOPEDLOCK_HPP
