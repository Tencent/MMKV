//
// Created by Ling Guo on 2018/5/25.
//

#include "InterProcessLock.h"
#include "MmapedFile.h"

InterProcessLock::InterProcessLock(const std::string &path) : m_mmapFile(nullptr) {
    m_mmapFile = new MmapedFile(path);
}

InterProcessLock::~InterProcessLock() {
    if (m_mmapFile) {
        delete m_mmapFile;
        m_mmapFile = nullptr;
    }
}

void InterProcessLock::initLock() {
    int32_t status = InitializingSegment;
    m_mmaped.m_initStatus.compare_exchange_strong(status, UninitializedSegment);
    if (status == UninitializedSegment) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        // TODO: robust mutex
        //pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        pthread_mutex_init(&m_mmaped.m_lock, &attr);

        pthread_mutexattr_destroy(&attr);

        m_mmaped.m_initStatus.store(InitializedSegment);
    } else if (status == InitializingSegment) {
        while (m_mmaped.m_initStatus.load() != InitializedSegment) {
            // sleep wait...
        }
        // return directory?
    } else if (status == InitializedSegment) {
        // return directory?
    }
}
