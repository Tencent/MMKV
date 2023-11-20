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

#ifdef MMKV_APPLE

#    include "CodedInputData.h"
#    include "CodedInputDataCrypt.h"
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

void MiniPBCoder::decodeOneMap(MMKVMap &dic, size_t position, bool greedy) {
    auto block = [position, this](MMKVMap &dictionary) {
        if (position) {
            m_inputData->seek(position);
        } else {
            m_inputData->readInt32();
        }
        while (!m_inputData->isAtEnd()) {
            KeyValueHolder kvHolder;
            const auto &key = m_inputData->readString(kvHolder);
            if (key.length > 0) {
                m_inputData->readData(kvHolder);
                auto itr = dictionary.find(key);
                if (itr != dictionary.end()) {
                    if (kvHolder.valueSize > 0) {
                        itr->second = std::move(kvHolder);
                    } else {
                        auto oldKey = itr->first;
                        dictionary.erase(itr);
                        [oldKey release];
                    }
                } else {
                    if (kvHolder.valueSize > 0) {
                        dictionary.emplace(key, std::move(kvHolder));
                        [key retain];
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
        } catch (...) {
            MMKVError("decode fail");
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
        } catch (...) {
            MMKVError("decode fail");
        }
    }
}

#    ifndef MMKV_DISABLE_CRYPT

void MiniPBCoder::decodeOneMap(MMKVMapCrypt &dic, size_t position, bool greedy) {
    auto block = [position, this](MMKVMapCrypt &dictionary) {
        if (position) {
            m_inputDataDecrpt->seek(position);
        } else {
            m_inputDataDecrpt->readInt32();
        }
        while (!m_inputDataDecrpt->isAtEnd()) {
            KeyValueHolderCrypt kvHolder;
            const auto &key = m_inputDataDecrpt->readString(kvHolder);
            if (key.length > 0) {
                m_inputDataDecrpt->readData(kvHolder);
                auto itr = dictionary.find(key);
                if (itr != dictionary.end()) {
                    if (kvHolder.realValueSize() > 0) {
                        itr->second = std::move(kvHolder);
                    } else {
                        auto oldKey = itr->first;
                        dictionary.erase(itr);
                        [oldKey release];
                    }
                } else {
                    if (kvHolder.realValueSize() > 0) {
                        dictionary.emplace(key, std::move(kvHolder));
                        [key retain];
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
        } catch (...) {
            MMKVError("decode fail");
        }
    } else {
        try {
            MMKVMapCrypt tmpDic;
            block(tmpDic);
            dic.swap(tmpDic);
            for (auto &pair : tmpDic) {
                [pair.first release];
            }
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        } catch (...) {
            MMKVError("decode fail");
        }
    }
}

#    endif // MMKV_DISABLE_CRYPT

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

#endif // MMKV_APPLE
