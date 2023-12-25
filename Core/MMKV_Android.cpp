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

#include "MMKV.h"

#ifdef MMKV_ANDROID

#    include "InterProcessLock.h"
#    include "KeyValueHolder.h"
#    include "MMKVLog.h"
#    include "MMKVMetaInfo.hpp"
#    include "MemoryFile.h"
#    include "ScopedLock.hpp"
#    include "ThreadLock.h"
#    include <unistd.h>
#    include "MMKV_IO.h"

using namespace std;
using namespace mmkv;

extern unordered_map<string, MMKV *> *g_instanceDic;
extern ThreadLock *g_instanceLock;

MMKV::MMKV(const string &mmapID, int size, MMKVMode mode, string *cryptKey, string *rootPath, size_t expectedCapacity)
    : m_mmapID((mode & MMKV_BACKUP) ? mmapID : mmapedKVKey(mmapID, rootPath)) // historically Android mistakenly use mmapKey as mmapID
    , m_path(mappedKVPathWithID(m_mmapID, mode, rootPath))
    , m_crcPath(crcPathWithID(m_mmapID, mode, rootPath))
    , m_dic(nullptr)
    , m_dicCrypt(nullptr)
    , m_expectedCapacity(std::max<size_t>(DEFAULT_MMAP_SIZE, roundUp<size_t>(expectedCapacity, DEFAULT_MMAP_SIZE)))
    , m_file(new MemoryFile(m_path, size, (mode & MMKV_ASHMEM) ? MMFILE_TYPE_ASHMEM : MMFILE_TYPE_FILE, m_expectedCapacity))
    , m_metaFile(new MemoryFile(m_crcPath, DEFAULT_MMAP_SIZE, m_file->m_fileType))
    , m_metaInfo(new MMKVMetaInfo())
    , m_crypter(nullptr)
    , m_lock(new ThreadLock())
    , m_fileLock(new FileLock(m_metaFile->getFd(), (mode & MMKV_ASHMEM)))
    , m_sharedProcessLock(new InterProcessLock(m_fileLock, SharedLockType))
    , m_exclusiveProcessLock(new InterProcessLock(m_fileLock, ExclusiveLockType))
    , m_isInterProcess((mode & MMKV_MULTI_PROCESS) != 0 || (mode & CONTEXT_MODE_MULTI_PROCESS) != 0) {
    m_actualSize = 0;
    m_output = nullptr;

    // force use fcntl(), otherwise will conflict with MemoryFile::reloadFromFile()
    m_fileModeLock = new FileLock(m_file->getFd(), true);
    m_sharedProcessModeLock = new InterProcessLock(m_fileModeLock, SharedLockType);
    m_exclusiveProcessModeLock = nullptr;

#    ifndef MMKV_DISABLE_CRYPT
    if (cryptKey && cryptKey->length() > 0) {
        m_dicCrypt = new MMKVMapCrypt();
        m_crypter = new AESCrypt(cryptKey->data(), cryptKey->length());
    } else
#    endif
    {
        m_dic = new MMKVMap();
    }

    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    m_crcDigest = 0;

    m_sharedProcessLock->m_enable = m_isInterProcess;
    m_exclusiveProcessLock->m_enable = m_isInterProcess;

    // sensitive zone
    /*{
        SCOPED_LOCK(m_sharedProcessLock);
        loadFromFile();
    }*/
}

MMKV::MMKV(const string &mmapID, int ashmemFD, int ashmemMetaFD, string *cryptKey)
    : m_mmapID(mmapID)
    , m_path(mappedKVPathWithID(m_mmapID, MMKV_ASHMEM, nullptr))
    , m_crcPath(crcPathWithID(m_mmapID, MMKV_ASHMEM, nullptr))
    , m_dic(nullptr)
    , m_dicCrypt(nullptr)
    , m_file(new MemoryFile(ashmemFD))
    , m_metaFile(new MemoryFile(ashmemMetaFD))
    , m_metaInfo(new MMKVMetaInfo())
    , m_crypter(nullptr)
    , m_lock(new ThreadLock())
    , m_fileLock(new FileLock(m_metaFile->getFd(), true))
    , m_sharedProcessLock(new InterProcessLock(m_fileLock, SharedLockType))
    , m_exclusiveProcessLock(new InterProcessLock(m_fileLock, ExclusiveLockType))
    , m_isInterProcess(true) {

    m_actualSize = 0;
    m_output = nullptr;

    // force use fcntl(), otherwise will conflict with MemoryFile::reloadFromFile()
    m_fileModeLock = new FileLock(m_file->getFd(), true);
    m_sharedProcessModeLock = new InterProcessLock(m_fileModeLock, SharedLockType);
    m_exclusiveProcessModeLock = nullptr;

#    ifndef MMKV_DISABLE_CRYPT
    if (cryptKey && cryptKey->length() > 0) {
        m_dicCrypt = new MMKVMapCrypt();
        m_crypter = new AESCrypt(cryptKey->data(), cryptKey->length());
    } else
#    endif
    {
        m_dic = new MMKVMap();
    }

    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    m_crcDigest = 0;

    m_sharedProcessLock->m_enable = m_isInterProcess;
    m_exclusiveProcessLock->m_enable = m_isInterProcess;

    // sensitive zone
    /*{
        SCOPED_LOCK(m_sharedProcessLock);
        loadFromFile();
    }*/
}

MMKV *MMKV::mmkvWithID(const string &mmapID, int size, MMKVMode mode, string *cryptKey, string *rootPath,
                       size_t expectedCapacity) {
    if (mmapID.empty()) {
        return nullptr;
    }
    SCOPED_LOCK(g_instanceLock);

    auto mmapKey = mmapedKVKey(mmapID, rootPath);
    auto itr = g_instanceDic->find(mmapKey);
    if (itr != g_instanceDic->end()) {
        MMKV *kv = itr->second;
        return kv;
    }
    if (rootPath) {
        if (!isFileExist(*rootPath)) {
            if (!mkPath(*rootPath)) {
                return nullptr;
            }
        }
        MMKVInfo("prepare to load %s (id %s) from rootPath %s", mmapID.c_str(), mmapKey.c_str(), rootPath->c_str());
    }
    auto kv = new MMKV(mmapID, size, mode, cryptKey, rootPath, expectedCapacity);
    (*g_instanceDic)[mmapKey] = kv;
    return kv;
}

MMKV *MMKV::mmkvWithAshmemFD(const string &mmapID, int fd, int metaFD, string *cryptKey) {

    if (fd < 0) {
        return nullptr;
    }
    SCOPED_LOCK(g_instanceLock);

    auto itr = g_instanceDic->find(mmapID);
    if (itr != g_instanceDic->end()) {
        MMKV *kv = itr->second;
#    ifndef MMKV_DISABLE_CRYPT
        kv->checkReSetCryptKey(fd, metaFD, cryptKey);
#    endif
        return kv;
    }
    auto kv = new MMKV(mmapID, fd, metaFD, cryptKey);
    (*g_instanceDic)[mmapID] = kv;
    return kv;
}

int MMKV::ashmemFD() {
    return (m_file->m_fileType & mmkv::MMFILE_TYPE_ASHMEM) ? m_file->getFd() : -1;
}

int MMKV::ashmemMetaFD() {
    return (m_file->m_fileType & mmkv::MMFILE_TYPE_ASHMEM) ? m_metaFile->getFd() : -1;
}

#    ifndef MMKV_DISABLE_CRYPT
void MMKV::checkReSetCryptKey(int fd, int metaFD, string *cryptKey) {
    SCOPED_LOCK(m_lock);

    checkReSetCryptKey(cryptKey);

    if (m_file->m_fileType & MMFILE_TYPE_ASHMEM) {
        if (m_file->getFd() != fd) {
            ::close(fd);
        }
        if (m_metaFile->getFd() != metaFD) {
            ::close(metaFD);
        }
    }
}
#    endif // MMKV_DISABLE_CRYPT

bool MMKV::checkProcessMode() {
    // avoid exception on open() error
    if (!m_file->isFileValid()) {
        return true;
    }

    if (m_isInterProcess) {
        if (!m_exclusiveProcessModeLock) {
            m_exclusiveProcessModeLock = new InterProcessLock(m_fileModeLock, ExclusiveLockType);
        }
        // avoid multiple processes get shared lock at the same time, https://github.com/Tencent/MMKV/issues/523
        auto tryAgain = false;
        auto exclusiveLocked = m_exclusiveProcessModeLock->try_lock(&tryAgain);
        if (exclusiveLocked) {
            return true;
        }
        auto shareLocked = m_sharedProcessModeLock->try_lock();
        if (!shareLocked) {
            // this call will fail on most case, just do it to make sure
            m_exclusiveProcessModeLock->try_lock();
            return true;
        } else {
            if (!tryAgain) {
                // something wrong with the OS/filesystem, let's try again
                exclusiveLocked = m_exclusiveProcessModeLock->try_lock(&tryAgain);
                if (!exclusiveLocked && !tryAgain) {
                    // still something wrong, we have to give up and assume it passed the test
                    MMKVWarning("Got a shared lock, but fail to exclusive lock [%s], assume it's ok", m_mmapID.c_str());
                    exclusiveLocked = true;
                }
            }
            if (!exclusiveLocked) {
                MMKVError("Got a shared lock, but fail to exclusive lock [%s]", m_mmapID.c_str());
            }
            return exclusiveLocked;
        }
    } else {
        auto tryAgain = false;
        auto shareLocked = m_sharedProcessModeLock->try_lock(&tryAgain);
        if (!shareLocked && !tryAgain) {
            // something wrong with the OS/filesystem, we have to give up and assume it passed the test
            MMKVWarning("Fail to shared lock [%s], assume it's ok", m_mmapID.c_str());
            shareLocked = true;
        }
        if (!shareLocked) {
            MMKVError("Fail to share lock [%s]", m_mmapID.c_str());
        }
        return shareLocked;
    }
}

#endif // MMKV_ANDROID
