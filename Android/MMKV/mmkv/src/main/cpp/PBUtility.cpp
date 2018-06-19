//
// Created by Ling Guo on 2018/4/25.
//

#include "PBUtility.h"

uint32_t pbRawVarint32Size(int32_t value) {
    if ((value & (0xffffffff << 7)) == 0) {
        return 1;
    } else if ((value & (0xffffffff << 14)) == 0) {
        return 2;
    } else if ((value & (0xffffffff << 21)) == 0) {
        return 3;
    } else if ((value & (0xffffffff << 28)) == 0) {
        return 4;
    }
    return 5;
}

uint32_t pbInt64Size(int64_t value) {
    if ((value & (0xffffffffffffffffL << 7)) == 0) {
        return 1;
    } else if ((value & (0xffffffffffffffffL << 14)) == 0) {
        return 2;
    } else if ((value & (0xffffffffffffffffL << 21)) == 0) {
        return 3;
    } else if ((value & (0xffffffffffffffffL << 28)) == 0) {
        return 4;
    } else if ((value & (0xffffffffffffffffL << 35)) == 0) {
        return 5;
    } else if ((value & (0xffffffffffffffffL << 42)) == 0) {
        return 6;
    } else if ((value & (0xffffffffffffffffL << 49)) == 0) {
        return 7;
    } else if ((value & (0xffffffffffffffffL << 56)) == 0) {
        return 8;
    } else if ((value & (0xffffffffffffffffL << 63)) == 0) {
        return 9;
    }
    return 10;
}
