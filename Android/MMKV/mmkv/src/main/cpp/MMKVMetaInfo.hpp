//
// Created by Ling Guo on 2018/5/31.
//

#ifndef MMKV_MMKVMETAINFO_H
#define MMKV_MMKVMETAINFO_H

#include <stdint.h>
#include <cstring>
#include <cassert>

struct MMKVMetaInfo {
    uint32_t m_crcDigest = 0;
    uint32_t m_version = 1;
    uint32_t m_sequence = 0;    // full write-back count

    void write(void* ptr) {
        assert (ptr);
        memcpy(ptr, this, sizeof(MMKVMetaInfo));
    }

    void read(const void* ptr) {
        assert (ptr);
        memcpy(this, ptr, sizeof(MMKVMetaInfo));
    }
};


#endif //MMKV_MMKVMETAINFO_H
