/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
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
#include "CodedInputData.h"
#include "CodedOutputData.h"
#include "MMBuffer.h"
#include "PBEncodeItem.hpp"
#include "PBUtility.h"
#include <string>
#include <vector>

#if __has_feature(objc_arc)
#    error This file must be compiled with MRC. Use -fno-objc-arc flag.
#endif

using namespace std;

namespace mmkv {

MiniPBCoder::MiniPBCoder() {
    m_inputBuffer = nullptr;
    m_inputData = nullptr;

    m_outputBuffer = nullptr;
    m_outputData = nullptr;
    m_encodeItems = nullptr;
}

MiniPBCoder::~MiniPBCoder() {
    delete m_inputData;
    delete m_outputBuffer;
    delete m_outputData;
    delete m_encodeItems;
}

MiniPBCoder::MiniPBCoder(const MMBuffer *inputBuffer) : MiniPBCoder() {
    m_inputBuffer = inputBuffer;
    m_inputData =
        new CodedInputData(m_inputBuffer->getPtr(), static_cast<int32_t>(m_inputBuffer->length()));
}

// encode

// write object using prepared m_encodeItems[]
void MiniPBCoder::writeRootObject() {
    for (size_t index = 0, total = m_encodeItems->size(); index < total; index++) {
        PBEncodeItem *encodeItem = &(*m_encodeItems)[index];
        switch (encodeItem->type) {
            case PBEncodeItemType_String: {
                m_outputData->writeString(*(encodeItem->value.strValue));
                break;
            }
            case PBEncodeItemType_Data: {
                m_outputData->writeData(*(encodeItem->value.bufferValue));
                break;
            }
            case PBEncodeItemType_Container: {
                m_outputData->writeRawVarint32(encodeItem->valueSize);
                break;
            }
#ifdef MMKV_IOS_OR_MAC
            case PBEncodeItemType_NSString: {
                m_outputData->writeRawVarint32(encodeItem->valueSize);
                if (encodeItem->valueSize > 0 && encodeItem->value.tmpObjectValue != nullptr) {
                    auto obj = (__bridge NSData *) encodeItem->value.tmpObjectValue;
                    MMBuffer buffer((void *) obj.bytes, obj.length, MMBufferNoCopy);
                    m_outputData->writeRawData(buffer);
                }
                break;
            }
            case PBEncodeItemType_NSData: {
                m_outputData->writeRawVarint32(encodeItem->valueSize);
                if (encodeItem->valueSize > 0 && encodeItem->value.objectValue != nullptr) {
                    auto obj = (__bridge NSData *) encodeItem->value.objectValue;
                    MMBuffer buffer((void *) obj.bytes, obj.length, MMBufferNoCopy);
                    m_outputData->writeRawData(buffer);
                }
                break;
            }
            case PBEncodeItemType_NSDate: {
                NSDate *oDate = (__bridge NSDate *) encodeItem->value.objectValue;
                m_outputData->writeDouble(oDate.timeIntervalSince1970);
                break;
            }
#endif // MMKV_IOS_OR_MAC
            case PBEncodeItemType_None: {
                MMKVError("%d", encodeItem->type);
                break;
            }
        }
    }
}

size_t MiniPBCoder::prepareObjectForEncode(const string &str) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem *encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_String;
        encodeItem->value.strValue = &str;
        encodeItem->valueSize = static_cast<int32_t>(str.size());
    }
    encodeItem->compiledSize = pbRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

size_t MiniPBCoder::prepareObjectForEncode(const MMBuffer &buffer) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem *encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_Data;
        encodeItem->value.bufferValue = &buffer;
        encodeItem->valueSize = static_cast<uint32_t>(buffer.length());
    }
    encodeItem->compiledSize = pbRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

size_t MiniPBCoder::prepareObjectForEncode(const vector<string> &v) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem *encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_Container;
        encodeItem->value.strValue = nullptr;

        for (const auto &str : v) {
            size_t itemIndex = prepareObjectForEncode(str);
            if (itemIndex < m_encodeItems->size()) {
                (*m_encodeItems)[index].valueSize += (*m_encodeItems)[itemIndex].compiledSize;
            }
        }

        encodeItem = &(*m_encodeItems)[index];
    }
    encodeItem->compiledSize = pbRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

size_t MiniPBCoder::prepareObjectForEncode(const MMKVMap &map) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem *encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_Container;
        encodeItem->value.strValue = nullptr;

        for (const auto &itr : map) {
            const auto &key = itr.first;
            const auto &value = itr.second;
#ifdef MMKV_IOS_OR_MAC
            if (key.length <= 0) {
#else
            if (key.length() <= 0) {
#endif
                continue;
            }

            size_t keyIndex = prepareObjectForEncode(key);
            if (keyIndex < m_encodeItems->size()) {
                size_t valueIndex = prepareObjectForEncode(value);
                if (valueIndex < m_encodeItems->size()) {
                    (*m_encodeItems)[index].valueSize += (*m_encodeItems)[keyIndex].compiledSize;
                    (*m_encodeItems)[index].valueSize += (*m_encodeItems)[valueIndex].compiledSize;
                } else {
                    m_encodeItems->pop_back(); // pop key
                }
            }
        }

        encodeItem = &(*m_encodeItems)[index];
    }
    encodeItem->compiledSize = pbRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

#ifdef MMKV_IOS_OR_MAC

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
        encodeItem->valueSize = static_cast<int32_t>(buffer.length);
    } else if ([obj isKindOfClass:[NSDate class]]) {
        NSDate *oDate = (NSDate *) obj;
        encodeItem->type = PBEncodeItemType_NSDate;
        encodeItem->value.objectValue = (__bridge void *) oDate;
        encodeItem->valueSize = pbDoubleSize(oDate.timeIntervalSince1970);
        encodeItem->compiledSize = encodeItem->valueSize;
        return index; // double has fixed compilesize
    } else if ([obj isKindOfClass:[NSData class]]) {
        NSData *oData = (NSData *) obj;
        encodeItem->type = PBEncodeItemType_NSData;
        encodeItem->value.objectValue = (__bridge void *) oData;
        encodeItem->valueSize = static_cast<int32_t>(oData.length);
    } else {
        m_encodeItems->pop_back();
        MMKVError("%@ not recognized", NSStringFromClass(obj.class));
        return m_encodeItems->size();
    }
    encodeItem->compiledSize = pbRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
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

#endif // MMKV_IOS_OR_MAC

MMBuffer MiniPBCoder::getEncodeData(const string &str) {
    m_encodeItems = new vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(str);
    PBEncodeItem *oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputBuffer = new MMBuffer(oItem->compiledSize);
        m_outputData = new CodedOutputData(m_outputBuffer->getPtr(), m_outputBuffer->length());

        writeRootObject();
    }

    return move(*m_outputBuffer);
}

MMBuffer MiniPBCoder::getEncodeData(const MMBuffer &buffer) {
    m_encodeItems = new vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(buffer);
    PBEncodeItem *oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputBuffer = new MMBuffer(oItem->compiledSize);
        m_outputData = new CodedOutputData(m_outputBuffer->getPtr(), m_outputBuffer->length());

        writeRootObject();
    }

    return move(*m_outputBuffer);
}

MMBuffer MiniPBCoder::getEncodeData(const vector<string> &v) {
    m_encodeItems = new vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(v);
    PBEncodeItem *oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputBuffer = new MMBuffer(oItem->compiledSize);
        m_outputData = new CodedOutputData(m_outputBuffer->getPtr(), m_outputBuffer->length());

        writeRootObject();
    }

    return move(*m_outputBuffer);
}

MMBuffer MiniPBCoder::getEncodeData(const MMKVMap &map) {
    m_encodeItems = new vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(map);
    PBEncodeItem *oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputBuffer = new MMBuffer(oItem->compiledSize);
        m_outputData = new CodedOutputData(m_outputBuffer->getPtr(), m_outputBuffer->length());

        writeRootObject();
    }

    return std::move(*m_outputBuffer);
}

// decode

string MiniPBCoder::decodeOneString() {
    return m_inputData->readString();
}

MMBuffer MiniPBCoder::decodeOneBytes() {
    return m_inputData->readData();
}

vector<string> MiniPBCoder::decodeOneSet() {
    vector<string> v;

    auto length = m_inputData->readInt32();

    while (!m_inputData->isAtEnd()) {
        auto value = m_inputData->readString();
        v.push_back(move(value));
    }

    return v;
}

void MiniPBCoder::decodeOneMap(MMKVMap &dic, size_t size, bool greedy) {
    auto block = [size, this](MMKVMap &dictionary) {
        if (size == 0) {
            auto length = m_inputData->readInt32();
        }
        while (!m_inputData->isAtEnd()) {
#ifdef MMKV_IOS_OR_MAC
            const auto &key = m_inputData->readNSString();
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
#else
            const auto &key = m_inputData->readString();
            if (key.length() > 0) {
                auto value = m_inputData->readData();
                if (value.length() > 0) {
                    dictionary[key] = move(value);
                } else {
                    dictionary.erase(key);
                }
            }
#endif
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
#ifdef MMKV_IOS_OR_MAC
            for (auto &pair : tmpDic) {
                [pair.first release];
            }
#endif
        } catch (std::exception &exception) {
            MMKVError("%s", exception.what());
        }
    }
}

string MiniPBCoder::decodeString(const MMBuffer &oData) {
    MiniPBCoder oCoder(&oData);
    return oCoder.decodeOneString();
}

MMBuffer MiniPBCoder::decodeBytes(const MMBuffer &oData) {
    MiniPBCoder oCoder(&oData);
    return oCoder.decodeOneBytes();
}

void MiniPBCoder::decodeMap(MMKVMap &dic, const MMBuffer &oData, size_t size) {
    MiniPBCoder oCoder(&oData);
    oCoder.decodeOneMap(dic, size, false);
}

void MiniPBCoder::greedyDecodeMap(MMKVMap &dic, const MMBuffer &oData, size_t size) {
    MiniPBCoder oCoder(&oData);
    oCoder.decodeOneMap(dic, size, true);
}

vector<string> MiniPBCoder::decodeSet(const MMBuffer &oData) {
    MiniPBCoder oCoder(&oData);
    return oCoder.decodeOneSet();
}

#ifdef MMKV_IOS_OR_MAC

NSObject *MiniPBCoder::decodeObject(const MMBuffer &oData, Class cls) {
    if (!cls || oData.length() == 0) {
        return nil;
    }
    CodedInputData input(oData.getPtr(), static_cast<int32_t>(oData.length()));
    if (cls == [NSString class]) {
        return input.readNSString();
    } else if (cls == [NSMutableString class]) {
        return [NSMutableString stringWithString:input.readNSString()];
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

#endif // MMKV_IOS_OR_MAC

} // namespace mmkv
