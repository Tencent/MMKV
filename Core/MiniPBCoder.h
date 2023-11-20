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

#include "KeyValueHolder.h"
#include "MMBuffer.h"
#include "MMBuffer.h"
#include "MMKVLog.h"
#include "PBUtility.h"
#include <cstdint>

namespace mmkv {

class CodedInputData;
class CodedOutputData;
class AESCrypt;
class CodedInputDataCrypt;
struct PBEncodeItem;

class MiniPBCoder {
    const MMBuffer *m_inputBuffer = nullptr;
    CodedInputData *m_inputData = nullptr;
    CodedInputDataCrypt *m_inputDataDecrpt = nullptr;

    MMBuffer *m_outputBuffer = nullptr;
    CodedOutputData *m_outputData = nullptr;
    std::vector<PBEncodeItem> *m_encodeItems = nullptr;

    MiniPBCoder();
    explicit MiniPBCoder(const MMBuffer *inputBuffer, AESCrypt *crypter = nullptr);
    ~MiniPBCoder();

    void writeRootObject();

    size_t prepareObjectForEncode(const MMKVVector &vec);
    size_t prepareObjectForEncode(const MMBuffer &buffer);

    template <typename T>
    MMBuffer getEncodeData(const T &obj) {
        size_t index = prepareObjectForEncode(obj);
        return writePreparedItems(index);
    }

    MMBuffer writePreparedItems(size_t index);

    void decodeOneMap(MMKVMap &dic, size_t position, bool greedy);
#ifndef MMKV_DISABLE_CRYPT
    void decodeOneMap(MMKVMapCrypt &dic, size_t position, bool greedy);
#endif

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
        } catch (...) {
            MMKVError("encode fail");
            return MMBuffer();
        }
    }

    // opt encoding a single MMBuffer
    static MMBuffer encodeDataWithObject(const MMBuffer &obj);

    // return empty result if there's any error
    static void decodeMap(MMKVMap &dic, const MMBuffer &oData, size_t position = 0);

    // decode as much data as possible before any error happens
    static void greedyDecodeMap(MMKVMap &dic, const MMBuffer &oData, size_t position = 0);

#ifndef MMKV_DISABLE_CRYPT
    // return empty result if there's any error
    static void decodeMap(MMKVMapCrypt &dic, const MMBuffer &oData, AESCrypt *crypter, size_t position = 0);

    // decode as much data as possible before any error happens
    static void greedyDecodeMap(MMKVMapCrypt &dic, const MMBuffer &oData, AESCrypt *crypter, size_t position = 0);
#endif // MMKV_DISABLE_CRYPT

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
