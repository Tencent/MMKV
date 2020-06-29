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
#ifdef __cplusplus

#include "MMKVPredef.h"

#include "CodedOutputData.h"
#include "KeyValueHolder.h"
#include "MMBuffer.h"
#include "MMBuffer.h"
#include "MMKVLog.h"
#include "PBEncodeItem.hpp"
#include "PBUtility.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmkv {

class CodedInputData;
class CodedOutputData;
class AESCrypt;
class CodedInputDataCrypt;

class MiniPBCoder {
    const MMBuffer *m_inputBuffer = nullptr;
    CodedInputData *m_inputData = nullptr;
    CodedInputDataCrypt *m_inputDataDecrpt = nullptr;

    MMBuffer *m_outputBuffer = nullptr;
    CodedOutputData *m_outputData = nullptr;
    std::vector<PBEncodeItem> *m_encodeItems = nullptr;

    MiniPBCoder() = default;
    explicit MiniPBCoder(const MMBuffer *inputBuffer, AESCrypt *crypter = nullptr);
    ~MiniPBCoder();

    void writeRootObject();

    size_t prepareObjectForEncode(const MMKVVector &vec);
    size_t prepareObjectForEncode(const MMBuffer &buffer);

    template <typename T>
    MMBuffer getEncodeData(const T &vec) {
        m_encodeItems = new std::vector<PBEncodeItem>();
        size_t index = prepareObjectForEncode(vec);
        PBEncodeItem *oItem = (index < m_encodeItems->size()) ? &(*m_encodeItems)[index] : nullptr;
        if (oItem && oItem->compiledSize > 0) {
            m_outputBuffer = new MMBuffer(oItem->compiledSize);
            m_outputData = new CodedOutputData(m_outputBuffer->getPtr(), m_outputBuffer->length());

            writeRootObject();
        }

        return std::move(*m_outputBuffer);
    }

    void decodeOneMap(MMKVMap &dic, size_t position, bool greedy);
    void decodeOneMap(MMKVMapCrypt &dic, size_t position, bool greedy);

#ifndef MMKV_APPLE
    size_t prepareObjectForEncode(const std::string &str);
    size_t prepareObjectForEncode(const std::vector<std::string> &vector);

    std::vector<std::string> decodeOneVector();
#else
    // NSString, NSData, NSDate
    size_t prepareObjectForEncode(__unsafe_unretained NSObject *obj);
#endif

public:
    template <typename T>
    static MMBuffer encodeDataWithObject(const T &obj) {
        try {
            MiniPBCoder pbcoder;
            return pbcoder.getEncodeData(obj);
        } catch (const std::exception &exception) {
            MMKVError("%s", exception.what());
            return MMBuffer();
        }
    }

    template <>
    MMBuffer encodeDataWithObject<MMBuffer>(const MMBuffer &obj) {
        try {
            auto valueSize = static_cast<uint32_t>(obj.length());
            auto compiledSize = pbRawVarint32Size(valueSize) + valueSize;
            MMBuffer result(compiledSize);
            CodedOutputData output(result.getPtr(), result.length());
            output.writeData(obj);
            return result;
        } catch (const std::exception &exception) {
            MMKVError("%s", exception.what());
            return MMBuffer();
        }
    }

    // return empty result if there's any error
    static void decodeMap(MMKVMap &dic, const MMBuffer &oData, size_t position = 0);

    // decode as much data as possible before any error happens
    static void greedyDecodeMap(MMKVMap &dic, const MMBuffer &oData, size_t position = 0);

    // return empty result if there's any error
    static void decodeMap(MMKVMapCrypt &dic, const MMBuffer &oData, AESCrypt *crypter, size_t position = 0);

    // decode as much data as possible before any error happens
    static void greedyDecodeMap(MMKVMapCrypt &dic, const MMBuffer &oData, AESCrypt *crypter, size_t position = 0);

#ifndef MMKV_APPLE
    static std::vector<std::string> decodeVector(const MMBuffer &oData);
#else
    // NSString, NSData, NSDate
    static NSObject *decodeObject(const MMBuffer &oData, Class cls);

    static bool isCompatibleClass(Class cls);
#endif

    // just forbid it for possibly misuse
    explicit MiniPBCoder(const MiniPBCoder &other) = delete;
    MiniPBCoder &operator=(const MiniPBCoder &other) = delete;
};

} // namespace mmkv

#endif
#endif //MMKV_MINIPBCODER_H
