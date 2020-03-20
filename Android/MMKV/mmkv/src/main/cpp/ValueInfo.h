//
// Created by 王凯 on 2020-03-20.
//

#ifndef MMKV_VALUEINFO_H
#define MMKV_VALUEINFO_H

#include "ValueType.h"
class ValueInfo {
public:
    ValueType valueType;
    void * valuePtr;
};


#endif //MMKV_VALUEINFO_H
