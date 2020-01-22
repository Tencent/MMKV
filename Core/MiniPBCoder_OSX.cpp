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

#include "MiniPBCoder.h"

#ifdef MMKV_IOS_OR_MAC

#    include "CodedInputData.h"
#    include "CodedOutputData.h"
#    include "MMBuffer.h"
#    include "PBEncodeItem.hpp"
#    include "PBUtility.h"
#    include <string>
#    include <vector>

#    if __has_feature(objc_arc)
#        error This file must be compiled with MRC. Use -fno-objc-arc flag.
#    endif

using namespace std;

namespace mmkv {

size_t MiniPBCoder::prepareObjectForEncode(__unsafe_unretained NSObject *obj) {
    if (!obj) {
        return m_encodeItems->size();
    }
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem *encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;

    if ([obj isKindOfClass:[NSString class]]) {
        NSString *str = (NSString *) obj;
        encodeItem->type = PBEncodeItemType_NSString;
        NSData *buffer = [[str dataUsingEncoding:NSUTF8StringEncoding] retain];
        encodeItem->value.tmpObjectValue = (__bridge void *) buffer;
        encodeItem->valueSize = static_cast<uint32_t>(buffer.length);
    } else if ([obj isKindOfClass:[NSDate class]]) {
        NSDate *oDate = (NSDate *) obj;
        encodeItem->type = PBEncodeItemType_NSDate;
        encodeItem->value.objectValue = (__bridge void *) oDate;
        encodeItem->valueSize = pbDoubleSize();
        encodeItem->compiledSize = encodeItem->valueSize;
        return index; // double has fixed compilesize
    } else if ([obj isKindOfClass:[NSData class]]) {
        NSData *oData = (NSData *) obj;
        encodeItem->type = PBEncodeItemType_NSData;
        encodeItem->value.objectValue = (__bridge void *) oData;
        encodeItem->valueSize = static_cast<uint32_t>(oData.length);
    } else {
        m_encodeItems->pop_back();
        MMKVError("%@ not recognized", NSStringFromClass(obj.class));
        return m_encodeItems->size();
    }
    encodeItem->compiledSize = pbRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

void MiniPBCoder::decodeOneMap(MMKVMap &dic, size_t size, bool greedy) {
    auto block = [size, this](MMKVMap &dictionary) {
        if (size == 0) {
            [[maybe_unused]] auto length = m_inputData->readInt32();
        }
        while (!m_inputData->isAtEnd()) {
            const auto &key = m_inputData->readString();
            if (key.length > 0) {
                auto value = m_inputData->readData();
                if (value.length() > 0) {
                    dictionary[key] = move(value);
                    [key retain];
                } else {
                    auto itr = dictionary.find(key);
                    if (itr != dictionary.end()) {
                        dictionary.erase(itr);
                        [itr->first release];
                    }
                }
            }
        }
    };

    if (greedy) {
        try {
            block(dic);
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    } else {
        try {
            MMKVMap tmpDic;
            block(tmpDic);
            dic.swap(tmpDic);
            for (auto &pair : tmpDic) {
                [pair.first release];
            }
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
}

MMBuffer MiniPBCoder::getEncodeData(__unsafe_unretained NSObject *obj) {
    m_encodeItems = new vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(obj);
    PBEncodeItem *oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputBuffer = new MMBuffer(oItem->compiledSize);
        m_outputData = new CodedOutputData(m_outputBuffer->getPtr(), m_outputBuffer->length());

        writeRootObject();
    }

    return move(*m_outputBuffer);
}

NSObject *MiniPBCoder::decodeObject(const MMBuffer &oData, Class cls) {
    if (!cls || oData.length() == 0) {
        return nil;
    }
    CodedInputData input(oData.getPtr(), oData.length());
    if (cls == [NSString class]) {
        return input.readString();
    } else if (cls == [NSMutableString class]) {
        return [NSMutableString stringWithString:input.readString()];
    } else if (cls == [NSData class]) {
        return input.readNSData();
    } else if (cls == [NSMutableData class]) {
        return [NSMutableData dataWithData:input.readNSData()];
    } else if (cls == [NSDate class]) {
        return [NSDate dateWithTimeIntervalSince1970:input.readDouble()];
    } else {
        MMKVError("%@ not recognized", NSStringFromClass(cls));
    }

    return nil;
}

bool MiniPBCoder::isCompatibleObject(NSObject *obj) {
    if ([obj isKindOfClass:[NSString class]]) {
        return true;
    }
    if ([obj isKindOfClass:[NSData class]]) {
        return true;
    }
    if ([obj isKindOfClass:[NSDate class]]) {
        return true;
    }

    return false;
}

bool MiniPBCoder::isCompatibleClass(Class cls) {
    if (cls == [NSString class]) {
        return true;
    }
    if (cls == [NSMutableString class]) {
        return true;
    }
    if (cls == [NSData class]) {
        return true;
    }
    if (cls == [NSMutableData class]) {
        return true;
    }
    if (cls == [NSDate class]) {
        return true;
    }

    return false;
}

} // namespace mmkv

#endif // MMKV_IOS_OR_MAC
