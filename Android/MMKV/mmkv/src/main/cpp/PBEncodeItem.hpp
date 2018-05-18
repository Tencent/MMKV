//
// Created by Ling Guo on 2018/4/26.
//

#ifndef MMKV_PBENCODEITEM_HPP
#define MMKV_PBENCODEITEM_HPP

#include <cstdint>
#include <memory.h>
#include <string>
#include "MMBuffer.h"

enum PBEncodeItemType {
    PBEncodeItemType_None,
    PBEncodeItemType_String,
    PBEncodeItemType_Data,
    PBEncodeItemType_Container,
};

struct PBEncodeItem
{
    PBEncodeItemType type;
    uint32_t compiledSize;
    uint32_t valueSize;
    union {
        const std::string* strValue;
        const MMBuffer* bufferValue;
    } value;

    PBEncodeItem()
            :type(PBEncodeItemType_None), compiledSize(0), valueSize(0)
    {
        memset(&value, 0, sizeof(value));
    }
};

#endif //MMKV_PBENCODEITEM_HPP
