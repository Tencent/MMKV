//
// Created by Ling Guo on 2018/5/25.
//

#include "InterProcessLock.h"
#include "MmapedFile.h"
#include "MMKVLog.h"

enum {
    UninitializedSegment = 0,
    InitializingSegment,
    InitializedSegment,
};

InterProcessLock::InterProcessLock(const std::string &path) : m_mmaped(nullptr), m_mmapFile(nullptr) {
    m_mmapFile = new MmapedFile(path);
    m_mmaped = reinterpret_cast<decltype(m_mmaped)>(m_mmapFile->getMemory());

    initLock();
}

InterProcessLock::~InterProcessLock() {
    if (m_mmapFile) {
        delete m_mmapFile;
        m_mmapFile = nullptr;
    }
    m_mmaped = nullptr;
}

void InterProcessLock::initLock() {
    int32_t status = UninitializedSegment;
    auto exchanged = m_mmaped->m_initStatus.compare_exchange_strong(status, InitializingSegment);
    if (exchanged /*&& status == UninitializedSegment*/) {
        MMKVInfo("Initializing %s ...", m_mmapFile->getName().c_str());

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        // TODO: robust mutex
        //pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        pthread_mutex_init(&m_mmaped->m_lock, &attr);

        pthread_mutexattr_destroy(&attr);

        m_mmaped->m_initStatus.store(InitializedSegment);

        MMKVInfo("Initialized %s", m_mmapFile->getName().c_str());
    } else if (status == InitializingSegment) {
        MMKVInfo("Waiting %s ...", m_mmapFile->getName().c_str());
        while (m_mmaped->m_initStatus.load() != InitializedSegment) {
            // sleep wait...
        }
        MMKVInfo("Done waiting %s ...", m_mmapFile->getName().c_str());
    } else if (status == InitializedSegment) {
        MMKVInfo("Someone already initialized %s", m_mmapFile->getName().c_str());
    } else {
        MMKVError("This should never happen, m_initStatus=%d", status);
    }
}

pthread_mutex_t* InterProcessLock::getLock() {
    if (m_mmaped) {
        return &m_mmaped->m_lock;
    }
}
