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

#ifndef MMKV_MINIPBCODER_H
#define MMKV_MINIPBCODER_H

#include "MMBuffer.h"
#include "MMKVLog.h"
#include "PBEncodeItem.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class CodedInputData;
class CodedOutputData;

class MiniPBCoder {
    const MMBuffer *m_inputBuffer;
    CodedInputData *m_inputData;

    MMBuffer *m_outputBuffer;
    CodedOutputData *m_outputData;
    std::vector<PBEncodeItem> *m_encodeItems;

private:
    MiniPBCoder();
    MiniPBCoder(const MMBuffer *inputBuffer);
    ~MiniPBCoder();

    void writeRootObject();
    size_t prepareObjectForEncode(const std::string &str);
    size_t prepareObjectForEncode(const MMBuffer &buffer);
    size_t prepareObjectForEncode(const std::vector<std::string> &vector);
    size_t prepareObjectForEncode(const std::unordered_map<std::string, MMBuffer> &map);

    MMBuffer getEncodeData(const std::string &str);
    MMBuffer getEncodeData(const MMBuffer &buffer);
    MMBuffer getEncodeData(const std::vector<std::string> &vector);
    MMBuffer getEncodeData(const std::unordered_map<std::string, MMBuffer> &map);

    std::string decodeOneString();
    MMBuffer decodeOneBytes();
    std::vector<std::string> decodeOneSet();
    void decodeOneMap(std::unordered_map<std::string, MMBuffer> &dic, size_t size = 0);

public:
    template <typename T>
    static MMBuffer encodeDataWithObject(const T &obj) {
        MiniPBCoder pbcoder;
        return pbcoder.getEncodeData(obj);
    }

    static std::string decodeString(const MMBuffer &oData);
    static MMBuffer decodeBytes(const MMBuffer &oData);
    static std::vector<std::string> decodeSet(const MMBuffer &oData);
    static void decodeMap(std::unordered_map<std::string, MMBuffer> &dic,
                          const MMBuffer &oData,
                          size_t size = 0);
};

#endif //MMKV_MINIPBCODER_H
