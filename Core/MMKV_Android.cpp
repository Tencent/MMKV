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

#    include "ScopedLock.hpp"
#    include <unistd.h>

using namespace std;
using namespace mmkv;

extern unordered_map<string, MMKV *> *g_instanceDic;
extern ThreadLock g_instanceLock;

extern string mmapedKVKey(const string &mmapID, string *relativePath);
extern string mappedKVPathWithID(const string &mmapID, MMKVMode mode, string *relativePath);
extern string crcPathWithID(const string &mmapID, MMKVMode mode, string *relativePath);

MMKV::MMKV(const string &mmapID, int size, MMKVMode mode, string *cryptKey, string *relativePath)
    : m_mmapID(mmapedKVKey(mmapID, relativePath))
    , m_path(mappedKVPathWithID(m_mmapID, mode, relativePath))
    , m_crcPath(crcPathWithID(m_mmapID, mode, relativePath))
    , m_file(m_path, size, (mode & MMKV_ASHMEM) ? MMFILE_TYPE_ASHMEM : MMFILE_TYPE_FILE)
    , m_metaFile(m_crcPath, DEFAULT_MMAP_SIZE, m_file.m_fileType)
    , m_crypter(nullptr)
    , m_fileLock(m_metaFile.getFd())
    , m_sharedProcessLock(&m_fileLock, SharedLockType)
    , m_exclusiveProcessLock(&m_fileLock, ExclusiveLockType)
    , m_isInterProcess((mode & MMKV_MULTI_PROCESS) != 0 || (mode & CONTEXT_MODE_MULTI_PROCESS) != 0) {
    m_actualSize = 0;
    m_output = nullptr;

    if (cryptKey && cryptKey->length() > 0) {
        m_crypter = new AESCrypt(cryptKey->data(), cryptKey->length());
    }

    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    m_crcDigest = 0;

    m_sharedProcessLock.m_enable = m_isInterProcess;
    m_exclusiveProcessLock.m_enable = m_isInterProcess;

    // sensitive zone
    {
        SCOPEDLOCK(m_sharedProcessLock);
        loadFromFile();
    }
}

MMKV::MMKV(const string &mmapID, int ashmemFD, int ashmemMetaFD, string *cryptKey)
    : m_mmapID(mmapID)
    , m_path("")
    , m_crcPath("")
    , m_file(ashmemFD)
    , m_metaFile(ashmemMetaFD)
    , m_crypter(nullptr)
    , m_fileLock(m_metaFile.getFd())
    , m_sharedProcessLock(&m_fileLock, SharedLockType)
    , m_exclusiveProcessLock(&m_fileLock, ExclusiveLockType)
    , m_isInterProcess(true) {

    // check mmapID with ashmemID
    {
        auto ashmemID = m_metaFile.getName();
        size_t pos = ashmemID.find_last_of('.');
        if (pos != string::npos) {
            ashmemID.erase(pos, string::npos);
        }
        if (mmapID != ashmemID) {
            MMKVWarning("mmapID[%s] != ashmem[%s]", mmapID.c_str(), ashmemID.c_str());
        }
    }
    m_path = string(ASHMEM_NAME_DEF) + "/" + m_mmapID;
    m_crcPath = string(ASHMEM_NAME_DEF) + "/" + m_metaFile.getName();
    m_actualSize = 0;
    m_output = nullptr;

    if (cryptKey && cryptKey->length() > 0) {
        m_crypter = new AESCrypt(cryptKey->data(), cryptKey->length());
    }

    m_needLoadFromFile = true;
    m_hasFullWriteback = false;

    m_crcDigest = 0;

    m_sharedProcessLock.m_enable = m_isInterProcess;
    m_exclusiveProcessLock.m_enable = m_isInterProcess;

    // sensitive zone
    {
        SCOPEDLOCK(m_sharedProcessLock);
        loadFromFile();
    }
}

MMKV *MMKV::mmkvWithID(const string &mmapID, int size, MMKVMode mode, string *cryptKey, string *relativePath) {

    if (mmapID.empty()) {
        return nullptr;
    }
    SCOPEDLOCK(g_instanceLock);

    auto mmapKey = mmapedKVKey(mmapID, relativePath);
    auto itr = g_instanceDic->find(mmapKey);
    if (itr != g_instanceDic->end()) {
        MMKV *kv = itr->second;
        return kv;
    }
    if (relativePath) {
        auto filePath = mappedKVPathWithID(mmapID, mode, relativePath);
        if (!isFileExist(filePath)) {
            if (!createFile(filePath)) {
                return nullptr;
            }
        }
        MMKVInfo("prepare to load %s (id %s) from relativePath %s", mmapID.c_str(), mmapKey.c_str(),
                 relativePath->c_str());
    }
    auto kv = new MMKV(mmapID, size, mode, cryptKey, relativePath);
    (*g_instanceDic)[mmapKey] = kv;
    return kv;
}

MMKV *MMKV::mmkvWithAshmemFD(const string &mmapID, int fd, int metaFD, string *cryptKey) {

    if (fd < 0) {
        return nullptr;
    }
    SCOPEDLOCK(g_instanceLock);

    auto itr = g_instanceDic->find(mmapID);
    if (itr != g_instanceDic->end()) {
        MMKV *kv = itr->second;
        kv->checkReSetCryptKey(fd, metaFD, cryptKey);
        return kv;
    }
    auto kv = new MMKV(mmapID, fd, metaFD, cryptKey);
    (*g_instanceDic)[mmapID] = kv;
    return kv;
}

void MMKV::checkReSetCryptKey(int fd, int metaFD, string *cryptKey) {
    SCOPEDLOCK(m_lock);

    checkReSetCryptKey(cryptKey);

    if (m_file.m_fileType & MMFILE_TYPE_ASHMEM) {
        if (m_file.getFd() != fd) {
            ::close(fd);
        }
        if (m_metaFile.getFd() != metaFD) {
            ::close(metaFD);
        }
    }
}

#endif
