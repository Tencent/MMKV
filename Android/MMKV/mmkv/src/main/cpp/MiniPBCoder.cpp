//
// Created by Ling Guo on 2018/4/26.
//

#include "MiniPBCoder.h"
#include <sys/stat.h>
#include <vector>
#include <string>
#include "WireFormat.h"
#include "CodedInputData.h"
#include "CodedOutputData.h"
#include "PBEncodeItem.hpp"
#include "MMBuffer.h"

using namespace std;

MiniPBCoder::MiniPBCoder() {
    m_inputData = nullptr;
    m_inputStream = nullptr;

    m_outputData = nullptr;
    m_outputStream = nullptr;
    m_encodeItems = nullptr;
}

MiniPBCoder::~MiniPBCoder() {
    if (m_inputStream) {
        delete m_inputStream;
    }
    if (m_outputData) {
        delete m_outputData;
    }
    if (m_outputStream) {
        delete m_outputStream;
    }
    if (m_encodeItems) {
        delete m_encodeItems;
    }
}

MiniPBCoder::MiniPBCoder(const MMBuffer* inputData) : MiniPBCoder() {
    m_inputData = inputData;
    m_inputStream = new CodedInputData(m_inputData->getPtr(), (int32_t)m_inputData->length());
}

#pragma mark - encode

// write object using prepared m_encodeItems[]
void MiniPBCoder::writeRootObject() {
    for (size_t index = 0, total = m_encodeItems->size(); index < total; index++) {
        PBEncodeItem* encodeItem = &(*m_encodeItems)[index];
        switch (encodeItem->type) {
            case PBEncodeItemType_String:
            {
                m_outputStream->writeString(*(encodeItem->value.strValue));
                break;
            }
            case PBEncodeItemType_Data:
            {
                m_outputStream->writeData(*(encodeItem->value.bufferValue));
                break;
            }
            case PBEncodeItemType_Container:
            {
                m_outputStream->writeRawVarint32(encodeItem->valueSize);
                break;
            }
            case PBEncodeItemType_None:
            {
                MMKVError("%d", encodeItem->type);
                break;
            }
        }
    }
}

size_t MiniPBCoder::prepareObjectForEncode(const std::string& str) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem* encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_String;
        encodeItem->value.strValue = &str;
        encodeItem->valueSize = static_cast<int32_t>(str.size());
    }
    encodeItem->compiledSize = computeRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

size_t MiniPBCoder::prepareObjectForEncode(const MMBuffer &buffer) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem* encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_Data;
        encodeItem->value.bufferValue = &buffer;
        encodeItem->valueSize = buffer.length();
    }
    encodeItem->compiledSize = computeRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

size_t MiniPBCoder::prepareObjectForEncode(const std::vector<std::string> &v) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem* encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_Container;
        encodeItem->value.strValue = nullptr;

        for(const auto& str : v) {
            size_t itemIndex = prepareObjectForEncode(str);
            if (itemIndex < m_encodeItems->size()) {
                (*m_encodeItems)[index].valueSize += (*m_encodeItems)[itemIndex].compiledSize;
            }
        }

        encodeItem = &(*m_encodeItems)[index];
    }
    encodeItem->compiledSize = computeRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

size_t MiniPBCoder::prepareObjectForEncode(const std::unordered_map<std::string, MMBuffer>& map) {
    m_encodeItems->push_back(PBEncodeItem());
    PBEncodeItem* encodeItem = &(m_encodeItems->back());
    size_t index = m_encodeItems->size() - 1;
    {
        encodeItem->type = PBEncodeItemType_Container;
        encodeItem->value.strValue = nullptr;

        for(const auto& itr : map) {
            const auto& key = itr.first;
            const auto& value = itr.second;
            if (key.length() <= 0) {
                continue;
            }

            size_t keyIndex = prepareObjectForEncode(key);
            if (keyIndex < m_encodeItems->size()) {
                size_t valueIndex = prepareObjectForEncode(value);
                if (valueIndex < m_encodeItems->size()) {
                    (*m_encodeItems)[index].valueSize += (*m_encodeItems)[keyIndex].compiledSize;
                    (*m_encodeItems)[index].valueSize += (*m_encodeItems)[valueIndex].compiledSize;
                } else {
                    m_encodeItems->pop_back();	// pop key
                }
            }
        }

        encodeItem = &(*m_encodeItems)[index];
    }
    encodeItem->compiledSize = computeRawVarint32Size(encodeItem->valueSize) + encodeItem->valueSize;

    return index;
}

MMBuffer MiniPBCoder::getEncodeData(const std::string& str) {
    m_encodeItems = new std::vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(str);
    PBEncodeItem* oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputData = new MMBuffer(oItem->compiledSize);
        m_outputStream = new CodedOutputData(m_outputData->getPtr(), m_outputData->length());

        writeRootObject();
    }

    return std::move(*m_outputData);
}

MMBuffer MiniPBCoder::getEncodeData(const MMBuffer &buffer) {
    m_encodeItems = new std::vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(buffer);
    PBEncodeItem* oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputData = new MMBuffer(oItem->compiledSize);
        m_outputStream = new CodedOutputData(m_outputData->getPtr(), m_outputData->length());

        writeRootObject();
    }

    return std::move(*m_outputData);
}

MMBuffer MiniPBCoder::getEncodeData(const std::vector<std::string> &v) {
    m_encodeItems = new std::vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(v);
    PBEncodeItem* oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputData = new MMBuffer(oItem->compiledSize);
        m_outputStream = new CodedOutputData(m_outputData->getPtr(), m_outputData->length());

        writeRootObject();
    }

    return std::move(*m_outputData);
}

MMBuffer MiniPBCoder::getEncodeData(const std::unordered_map<std::string, MMBuffer>& map) {
    m_encodeItems = new std::vector<PBEncodeItem>();
    size_t index = prepareObjectForEncode(map);
    PBEncodeItem* oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
    if (oItem && oItem->compiledSize > 0) {
        m_outputData = new MMBuffer(oItem->compiledSize);
        m_outputStream = new CodedOutputData(m_outputData->getPtr(), m_outputData->length());

        writeRootObject();
    }

    return std::move(*m_outputData);
}

MMBuffer MiniPBCoder::encodeDataWithObject(const std::string& str) {
    try {
        MiniPBCoder pbcoder;
        auto data = pbcoder.getEncodeData(str);
        return data;
    } catch (const std::exception& e) {
        MMKVError("%s", e.what());
    }

    return MMBuffer(0);
}

MMBuffer MiniPBCoder::encodeDataWithObject(const MMBuffer &buffer) {
    try {
        MiniPBCoder pbcoder;
        auto data = pbcoder.getEncodeData(buffer);
        return data;
    } catch (const std::exception& e) {
        MMKVError("%s", e.what());
    }

    return MMBuffer(0);
}

MMBuffer MiniPBCoder::encodeDataWithObject(const std::vector<std::string> &v) {
    try {
        MiniPBCoder pbcoder;
        auto data = pbcoder.getEncodeData(v);
        return data;
    } catch (const std::exception& e) {
        MMKVError("%s", e.what());
    }

    return MMBuffer(0);
}

MMBuffer MiniPBCoder::encodeDataWithObject(const std::unordered_map<std::string, MMBuffer>& map) {
    try {
        MiniPBCoder pbcoder;
        auto data = pbcoder.getEncodeData(map);
        return data;
    } catch (const std::exception& e) {
        MMKVError("%s", e.what());
    }

    return MMBuffer(0);
}

#pragma mark - decode

std::string MiniPBCoder::decodeOneString() {
    return m_inputStream->readString();
}

MMBuffer MiniPBCoder::decodeOneBytes() {
    return m_inputStream->readData();
}

std::vector<std::string> MiniPBCoder::decodeOneSet() {
    vector<string> v;

    int32_t length = m_inputStream->readRawVarint32();
    int32_t	limit = m_inputStream->pushLimit(static_cast<int32_t>(m_inputData->length()) - computeRawVarint32Size(length));

    while (!m_inputStream->isAtEnd()) {
        const auto& value = m_inputStream->readString();
        v.push_back(std::move(value));
    }
    m_inputStream->popLimit(limit);

    return v;
}

std::unordered_map<std::string, MMBuffer> MiniPBCoder::decodeOneMap() {
    std::unordered_map<std::string, MMBuffer> dic;

    int32_t length = m_inputStream->readRawVarint32();
    int32_t	limit = m_inputStream->pushLimit(static_cast<int32_t>(m_inputData->length()) - computeRawVarint32Size(length));

    while (!m_inputStream->isAtEnd()) {
        const auto& key = m_inputStream->readString();
        if (key.length() > 0) {
            auto value = m_inputStream->readData();
            if (value.length() > 0) {
                dic[key] = std::move(value);
            } else {
                dic.erase(key);
            }
        }
    }
    m_inputStream->popLimit(limit);

    return dic;
}

string MiniPBCoder::decodeString(const MMBuffer& oData) {
    string obj;
    try {
        MiniPBCoder oCoder(&oData);
        obj = oCoder.decodeOneString();
    } catch(const exception& e) {
        MMKVError("%s", e.what());
    }
    return obj;
}

MMBuffer MiniPBCoder::decodeBytes(const MMBuffer &oData) {
    try {
        MiniPBCoder oCoder(&oData);
        return oCoder.decodeOneBytes();
    } catch(const exception& e) {
        MMKVError("%s", e.what());
    }
    return MMBuffer(0);
}

std::unordered_map<std::string, MMBuffer> MiniPBCoder::decodeMap(const MMBuffer& oData) {
    std::unordered_map<std::string, MMBuffer> dic;
    try {
        MiniPBCoder oCoder(&oData);
        dic = oCoder.decodeOneMap();
    } catch(const exception& e) {
        MMKVError("%s", e.what());
    }
    return dic;
}

std::vector<std::string> MiniPBCoder::decodeSet(const MMBuffer &oData) {
    vector<string> v;
    try {
        MiniPBCoder oCoder(&oData);
        v = oCoder.decodeOneSet();
    } catch(const exception& e) {
        MMKVError("%s", e.what());
    }
    return v;
}
