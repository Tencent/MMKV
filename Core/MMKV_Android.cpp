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
// #include <bits/alltypes.h>

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

#ifndef MMKV_OHOS
static bool g_enableProcessModeCheck = false;
#endif

MMKV::MMKV(const string &mmapID, int size, MMKVMode mode, const string *cryptKey, const string *rootPath, size_t expectedCapacity)
    : m_mmapID(mmapID)
    , m_mode(mode)
    , m_path(mappedKVPathWithID(m_mmapID, rootPath, mode))
    , m_crcPath(crcPathWithPath(m_path))
    , m_dic(nullptr)
    , m_dicCrypt(nullptr)
    , m_expectedCapacity(std::max<size_t>(DEFAULT_MMAP_SIZE, roundUp<size_t>(expectedCapacity, DEFAULT_MMAP_SIZE)))
    , m_file(new MemoryFile(m_path, size, isAshmem() ? MMFILE_TYPE_ASHMEM : MMFILE_TYPE_FILE, m_expectedCapacity, isReadOnly(), !isAshmem()))
    , m_metaFile(new MemoryFile(m_crcPath, DEFAULT_MMAP_SIZE, m_file->m_fileType, 0, isReadOnly()))
    , m_metaInfo(new MMKVMetaInfo())
    , m_crypter(nullptr)
    , m_lock(new ThreadLock())
    , m_fileLock(new FileLock(m_metaFile->getFd(), isAshmem(), 0, 1))
    , m_sharedProcessLock(new InterProcessLock(m_fileLock, SharedLockType))
    , m_exclusiveProcessLock(new InterProcessLock(m_fileLock, ExclusiveLockType)) {
    m_actualSize = 0;
    m_output = nullptr;

#ifndef MMKV_OHOS
    if (g_enableProcessModeCheck) {
        // force use fcntl(), otherwise will conflict with MemoryFile::reloadFromFile()
        m_fileModeLock = new FileLock(m_metaFile->getFd(), true, 1, 2);
        m_sharedProcessModeLock = new InterProcessLock(m_fileModeLock, SharedLockType);
    } else {
        m_fileModeLock = nullptr;
        m_sharedProcessModeLock = nullptr;
    }
    m_exclusiveProcessModeLock = nullptr;
#endif

    m_fileMigrationLock = new FileLock(m_metaFile->getFd(), true, 2, 3);
    m_sharedMigrationLock = new InterProcessLock(m_fileMigrationLock, SharedLockType);
    m_sharedMigrationLock->try_lock();

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

    m_sharedProcessLock->m_enable = isMultiProcess();
    m_exclusiveProcessLock->m_enable = isMultiProcess();

    // sensitive zone
    /*{
        SCOPED_LOCK(m_sharedProcessLock);
        loadFromFile();
    }*/
}

MMKV::MMKV(const string &mmapID, int ashmemFD, int ashmemMetaFD, const string *cryptKey)
    : m_mmapID(mmapID)
    , m_mode(MMKV_ASHMEM)
    , m_path(mappedKVPathWithID(m_mmapID, nullptr, MMKV_ASHMEM))
    , m_crcPath(crcPathWithPath(m_path))
    , m_dic(nullptr)
    , m_dicCrypt(nullptr)
    , m_expectedCapacity(DEFAULT_MMAP_SIZE)
    , m_file(new MemoryFile(ashmemFD))
    , m_metaFile(new MemoryFile(ashmemMetaFD))
    , m_metaInfo(new MMKVMetaInfo())
    , m_crypter(nullptr)
    , m_lock(new ThreadLock())
    , m_fileLock(new FileLock(m_metaFile->getFd(), true, 0, 1))
    , m_sharedProcessLock(new InterProcessLock(m_fileLock, SharedLockType))
    , m_exclusiveProcessLock(new InterProcessLock(m_fileLock, ExclusiveLockType)) {

    m_actualSize = 0;
    m_output = nullptr;

#ifndef MMKV_OHOS
    if (g_enableProcessModeCheck) {
        // force use fcntl(), otherwise will conflict with MemoryFile::reloadFromFile()
        m_fileModeLock = new FileLock(m_metaFile->getFd(), true, 1, 2);
        m_sharedProcessModeLock = new InterProcessLock(m_fileModeLock, SharedLockType);
    } else {
        m_fileModeLock = nullptr;
        m_sharedProcessModeLock = nullptr;
    }
    m_exclusiveProcessModeLock = nullptr;
#endif

    m_fileMigrationLock = new FileLock(m_metaFile->getFd(), true, 2, 3);
    m_sharedMigrationLock = new InterProcessLock(m_fileMigrationLock, SharedLockType);
    m_sharedMigrationLock->try_lock();

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

    m_sharedProcessLock->m_enable = true;
    m_exclusiveProcessLock->m_enable = true;

    // sensitive zone
    /*{
        SCOPED_LOCK(m_sharedProcessLock);
        loadFromFile();
    }*/
}

// historically Android mistakenly use mmapKey as mmapID, we try migrate back to normal when possible
MigrateStatus tryMigrateLegacyMMKVFile(const string &mmapID, const string *rootPath) {
    auto legacyID = legacyMmapedKVKey(mmapID, rootPath);
    if (legacyID == mmapID) {
        // it's not specially encoded
        return MigrateStatus::NotSpecial;
    }
    auto path = mappedKVPathWithID(legacyID, rootPath);
    auto targetPath = mappedKVPathWithID(mmapID, rootPath);
    bool oldExit = isFileExist(path);
    bool newExist = isFileExist(targetPath);
    if (oldExit) {
        if (newExist) {
            MMKVWarning("both legacy file [%s] modify: %lld ms, and new file [%s] modify: %lld ms exist",
                        path.c_str(), getFileModifyTimeInMS(path.c_str()),
                        targetPath.c_str(), getFileModifyTimeInMS(targetPath.c_str()));
            return MigrateStatus::OldAndNewExist;
        }
        auto file = File(path, OpenFlag::ReadWrite);
        if (file.isFileValid()) {
            // check if it's opened by other process
            auto fileMigrationLock = FileLock(file.getFd(), true, 2, 3);
            auto exclusiveMigrationLock = InterProcessLock(&fileMigrationLock, ExclusiveLockType);
            // works even if it's opened by us
            if (exclusiveMigrationLock.try_lock()) {
                if (tryAtomicRename(path, targetPath)) {
                    if (tryAtomicRename(crcPathWithPath(path), crcPathWithPath(targetPath))) {
                        MMKVInfo("Migrated legacy MMKV [%s] to [%s] in path %s", legacyID.c_str(), mmapID.c_str(), rootPath->c_str());
                        return MigrateStatus::OldToNewMigrated;
                    }
                }
            } else {
                MMKVInfo("Can't migrate legacy MMKV [%s] to [%s] in path %s, try next time.", legacyID.c_str(), mmapID.c_str(), rootPath->c_str());
            }
        }
        return MigrateStatus::OldToNewMigrateFail;
    }
    return newExist ? MigrateStatus::NewExist : MigrateStatus::NoneExist;
}

MMKV *MMKV::mmkvWithID(const string &mmapID, int size, MMKVMode mode, const string *cryptKey, const string *rootPath, size_t expectedCapacity) {
    if (mmapID.empty() || !g_instanceLock) {
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

    MMKV *kv = nullptr;
    auto migrateStatus = (mode & MMKV_ASHMEM) ? MigrateStatus::NoneExist : tryMigrateLegacyMMKVFile(mmapID, rootPath);
    switch (migrateStatus) {
        case MigrateStatus::NotSpecial:
        case MigrateStatus::NoneExist:
        case MigrateStatus::NewExist:
        case MigrateStatus::OldToNewMigrated:
        case MigrateStatus::OldAndNewExist: // TODO: shell we compare and use the latest one?
            kv = new MMKV(mmapID, size, mode, cryptKey, rootPath, expectedCapacity);
            break;
        case MigrateStatus::OldToNewMigrateFail: {
            auto legacyID = legacyMmapedKVKey(mmapID, rootPath);
            kv = new MMKV(legacyID, size, mode, cryptKey, rootPath, expectedCapacity);
            break;
        }
    }
    kv->m_mmapKey = mmapKey;

    (*g_instanceDic)[mmapKey] = kv;
    return kv;
}

MMKV *MMKV::mmkvWithAshmemFD(const string &mmapID, int fd, int metaFD, const string *cryptKey) {

    if (fd < 0 || !g_instanceLock) {
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
    kv->m_mmapKey = mmapID;
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
void MMKV::checkReSetCryptKey(int fd, int metaFD, const string *cryptKey) {
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

#ifndef MMKV_OHOS

bool MMKV::checkProcessMode() {
    // avoid exception on open() error
    if (!m_file->isFileValid()) {
        return true;
    }
    // avoid invalid status
    if (!g_enableProcessModeCheck || !m_fileModeLock || !m_sharedProcessModeLock) {
        return true;
    }

    if (isMultiProcess()) {
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

void MMKV::enableDisableProcessMode(bool enable) {
    MMKVInfo("process mode check enable/disable: %d", enable);
    g_enableProcessModeCheck = enable;
}

#endif // !MMKV_OHOS

MMKV *NameSpace::mmkvWithID(const string &mmapID, int size, MMKVMode mode, const string *cryptKey, size_t expectedCapacity) {
    return MMKV::mmkvWithID(mmapID, size, mode, cryptKey, &m_rootDir, expectedCapacity);
}

#endif // MMKV_ANDROID
