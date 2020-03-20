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

#ifndef MMKV_NATIVE_BRIDGE_H
#define MMKV_NATIVE_BRIDGE_H

#include <string>
#include <jni.h>
#include <__locale>
#include <locale>
#include "MMBuffer.h"

enum MMKVRecoverStrategic : int {
    OnErrorDiscard = 0,
    OnErrorRecover,
};

namespace mmkv {

MMKVRecoverStrategic onMMKVCRCCheckFail(const std::string &mmapID);

MMKVRecoverStrategic onMMKVFileLengthError(const std::string &mmapID);

void mmkvLog(int level,
             const std::string &file,
             int line,
             const std::string &function,
             const std::string &message);

void onContentChangedByOuterProcess(const std::string &mmapID);

jobject cInt2JavaInteger(JNIEnv *env,const int &value);
jobject cFloat2JavaFloat(JNIEnv *env,const float &value);
jobject cDouble2JavaDouble(JNIEnv *env,const double &value);
jobject cBool2JavaBool(JNIEnv *env,const bool &value);
jobject cLong2JavaLong(JNIEnv *env,const long &value);
jobject vector2javaSet(JNIEnv *env, const std::vector<std::string> &arr);
jstring string2jstring(JNIEnv *env, const std::string &str);
jbyteArray buffer2byteArray(JNIEnv *env, const MMBuffer &buffer);
} // namespace mmkv

extern int g_android_api;

#endif //MMKV_NATIVE_BRIDGE_H
