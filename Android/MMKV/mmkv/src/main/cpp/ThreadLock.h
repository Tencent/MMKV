//
// Created by Ling Guo on 2018/5/28.
//

#ifndef MMKV_THREADLOCK_H
#define MMKV_THREADLOCK_H

#include <pthread.h>

class ThreadLock {
private:
    pthread_mutex_t m_lock;

public:
    ThreadLock();
    ~ThreadLock();

    pthread_mutex_t* getLock(){
        return &m_lock;
    }
};


#endif //MMKV_THREADLOCK_H
