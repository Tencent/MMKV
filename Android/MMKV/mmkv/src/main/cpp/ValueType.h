//
// Created by 王凯 on 2020-03-18.
//

#ifndef MMKV_VALUETYPE_H
#define MMKV_VALUETYPE_H


#include <cstdint>

enum ValueType : uint8_t {

    Integer = 0,
    Boolean = 1 ,
    Long = 2 ,
    Float = 3 ,
    Double = 4 ,
    String = 5 ,
    Set = 6,
    Bytes = 7
};


#endif //MMKV_VALUETYPE_H
