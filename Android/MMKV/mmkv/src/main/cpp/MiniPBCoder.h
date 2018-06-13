//
// Created by Ling Guo on 2018/4/26.
//

#ifndef MMKV_MINIPBCODER_H
#define MMKV_MINIPBCODER_H

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "PBEncodeItem.hpp"
#include "MMKVLog.h"
#include "MMBuffer.h"

class CodedInputData;
class CodedOutputData;

class MiniPBCoder {
    const MMBuffer* m_inputData;
    CodedInputData* m_inputStream;

    MMBuffer* m_outputData;
    CodedOutputData* m_outputStream;
    std::vector<PBEncodeItem>* m_encodeItems;

private:
    MiniPBCoder();
    MiniPBCoder(const MMBuffer* inputData);
    ~MiniPBCoder();

    void writeRootObject();
    size_t prepareObjectForEncode(const std::string& str);
    size_t prepareObjectForEncode(const MMBuffer& buffer);
    size_t prepareObjectForEncode(const std::vector<std::string>& vector);
    size_t prepareObjectForEncode(const std::unordered_map<std::string, MMBuffer>& map);

    MMBuffer getEncodeData(const std::string& str);
    MMBuffer getEncodeData(const MMBuffer& buffer);
    MMBuffer getEncodeData(const std::vector<std::string>& vector);
    MMBuffer getEncodeData(const std::unordered_map<std::string, MMBuffer>& map);

    std::string decodeOneString();
    MMBuffer decodeOneBytes();
    std::vector<std::string> decodeOneSet();
    std::unordered_map<std::string, MMBuffer> decodeOneMap(size_t size = 0);

public:

    template <typename T>
    static MMBuffer encodeDataWithObject(const T& obj) {
        MiniPBCoder pbcoder;
        return pbcoder.getEncodeData(obj);
    }

    static std::string decodeString(const MMBuffer& oData);
    static MMBuffer decodeBytes(const MMBuffer& oData);
    static std::vector<std::string> decodeSet(const MMBuffer& oData);
    static std::unordered_map<std::string, MMBuffer> decodeMap(const MMBuffer& oData, size_t size = 0);
};


#endif //MMKV_MINIPBCODER_H
