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

#include "MMKVPredef.h"

#include "MMBuffer.h"
#include "MMKVLog.h"
#include "PBEncodeItem.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmkv {

class CodedInputData;
class CodedOutputData;

class MiniPBCoder {
    const MMBuffer *m_inputBuffer = nullptr;
    CodedInputData *m_inputData = nullptr;

    MMBuffer *m_outputBuffer = nullptr;
    CodedOutputData *m_outputData = nullptr;
    std::vector<PBEncodeItem> *m_encodeItems = nullptr;

    MiniPBCoder() = default;
    explicit MiniPBCoder(const MMBuffer *inputBuffer);
    ~MiniPBCoder();

    void writeRootObject();

    size_t prepareObjectForEncode(const MMKVMap &map);
    size_t prepareObjectForEncode(const MMBuffer &buffer);

    MMBuffer getEncodeData(const MMKVMap &map);

    void decodeOneMap(MMKVMap &dic, size_t size, bool greedy);

#ifndef MMKV_IOS_OR_MAC
    size_t prepareObjectForEncode(const std::string &str);
    size_t prepareObjectForEncode(const std::vector<std::string> &vector);

    MMBuffer getEncodeData(const std::string &str);
    MMBuffer getEncodeData(const MMBuffer &buffer);
    MMBuffer getEncodeData(const std::vector<std::string> &vector);

    std::string decodeOneString();
    MMBuffer decodeOneBytes();
    std::vector<std::string> decodeOneSet();
#else
    // NSString, NSData, NSDate
    size_t prepareObjectForEncode(__unsafe_unretained NSObject *obj);
    MMBuffer getEncodeData(__unsafe_unretained NSObject *obj);
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

    // return empty result if there's any error
    static void decodeMap(MMKVMap &dic, const MMBuffer &oData, size_t size = 0);

    // decode as much data as possible before any error happens
    static void greedyDecodeMap(MMKVMap &dic, const MMBuffer &oData, size_t size = 0);

#ifndef MMKV_IOS_OR_MAC
    static std::string decodeString(const MMBuffer &oData);

    static MMBuffer decodeBytes(const MMBuffer &oData);

    static std::vector<std::string> decodeSet(const MMBuffer &oData);
#else
    // NSString, NSData, NSDate
    static NSObject *decodeObject(const MMBuffer &oData, Class cls);

    static bool isCompatibleObject(NSObject *obj);
    static bool isCompatibleClass(Class cls);
#endif

    // just forbid it for possibly misuse
    explicit MiniPBCoder(const MiniPBCoder &other) = delete;
    MiniPBCoder &operator=(const MiniPBCoder &other) = delete;
};

} // namespace mmkv

#endif //MMKV_MINIPBCODER_H
