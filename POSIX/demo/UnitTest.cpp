/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

#include "MMKV.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>
#include <unistd.h>

using namespace std;
using namespace mmkv;

static const string KeyNotExist = "KeyNotExist";

void testBool(MMKV *mmkv) {
    auto ret = mmkv->set(true, "bool");
    assert(ret);

    auto value = mmkv->getBool("bool");
    assert(value);

    value = mmkv->getBool(KeyNotExist);
    assert(!value);

    value = mmkv->getBool(KeyNotExist, true);
    assert(value);

    printf("test bool: passed\n");
}

void testInt32(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<int32_t>::max(), "int32");
    assert(ret);

    auto value = mmkv->getInt32("int32");
    assert(value == numeric_limits<int32_t>::max());

    value = mmkv->getInt32(KeyNotExist);
    assert(value == 0);

    value = mmkv->getInt32(KeyNotExist, -1);
    assert(value == -1);

    printf("test int32: passed\n");
}

void testUInt32(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<uint32_t>::max(), "uint32");
    assert(ret);

    auto value = mmkv->getUInt32("uint32");
    assert(value == numeric_limits<uint32_t>::max());

    value = mmkv->getUInt32(KeyNotExist);
    assert(value == 0);

    value = mmkv->getUInt32(KeyNotExist, -1);
    assert(value == -1);

    printf("test uint32: passed\n");
}

void testInt64(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<int64_t>::min(), "int64");
    assert(ret);

    auto value = mmkv->getInt64("int64");
    assert(value == numeric_limits<int64_t>::min());

    value = mmkv->getInt64(KeyNotExist);
    assert(value == 0);

    value = mmkv->getInt64(KeyNotExist, -1);
    assert(value == -1);

    printf("test int64: passed\n");
}

void testUInt64(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<uint64_t>::max(), "uint64");
    assert(ret);

    auto value = mmkv->getUInt64("uint64");
    assert(value == numeric_limits<uint64_t>::max());

    value = mmkv->getUInt64(KeyNotExist);
    assert(value == 0);

    value = mmkv->getUInt64(KeyNotExist, -1);
    assert(value == -1);

    printf("test uint64: passed\n");
}

template <typename T>
bool EqualWithAccuracy(T value1, T value2, T accuracy) {
    return fabs(value1 - value2) <= accuracy;
}

void testFloat(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<float>::max(), "float");
    assert(ret);

    auto value = mmkv->getFloat("float");
    assert(EqualWithAccuracy(value, numeric_limits<float>::max(), 0.001f));

    value = mmkv->getFloat(KeyNotExist);
    assert(EqualWithAccuracy(value, 0.0f, 0.001f));

    value = mmkv->getFloat(KeyNotExist, -1.0f);
    assert(EqualWithAccuracy(value, -1.0f, 0.001f));

    printf("test float: passed\n");
}

void testDouble(MMKV *mmkv) {
    auto ret = mmkv->set(numeric_limits<double>::max(), "double");
    assert(ret);

    auto value = mmkv->getDouble("double");
    assert(EqualWithAccuracy(value, numeric_limits<double>::max(), 0.001));

    value = mmkv->getDouble(KeyNotExist);
    assert(EqualWithAccuracy(value, 0.0, 0.001));

    value = mmkv->getDouble(KeyNotExist, -1.0);
    assert(EqualWithAccuracy(value, -1.0, 0.001));

    printf("test double: passed\n");
}

void testString(MMKV *mmkv) {
    string str = "Hello 2018 world cup 世界杯";
    auto ret = mmkv->set(str, "string");
    assert(ret);

    string value;
    ret = mmkv->getString("string", value);
    assert(ret && str == value);

    const char *cString = "Hello 2022 world cup 世界杯";
    ret = mmkv->set(cString, "cstring");
    assert(ret);

    ret = mmkv->getString("cstring", value);
    assert(ret && value == cString);

    ret = mmkv->getString(KeyNotExist, value);
    assert(!ret);

    printf("test string: passed\n");
}

void testBytes(MMKV *mmkv) {
    string str = "Hello 2018 world cup 世界杯";
    MMBuffer buffer((void *) str.data(), str.length(), MMBufferNoCopy);
    auto ret = mmkv->set(buffer, "bytes");
    assert(ret);

    auto value = mmkv->getBytes("bytes");
    assert(value.length() == buffer.length() && memcmp(value.getPtr(), buffer.getPtr(), value.length()) == 0);

    value = mmkv->getBytes(KeyNotExist);
    assert(value.length() == 0);

    printf("test bytes: passed\n");
}

void testVector(MMKV *mmkv) {
    vector<string> v = {"1", "0", "2", "4"};
    auto ret = mmkv->set(v, "vector");
    assert(ret);

    vector<string> value;
    ret = mmkv->getVector("vector", value);
    assert(ret && value == v);

    printf("test vector: passed\n");
}

void testRemove(MMKV *mmkv) {
    auto ret = mmkv->set(true, "bool_1");
    ret &= mmkv->set(numeric_limits<int32_t>::max(), "int_1");
    ret &= mmkv->set(numeric_limits<int64_t>::max(), "long_1");
    ret &= mmkv->set(numeric_limits<float>::min(), "float_1");
    ret &= mmkv->set(numeric_limits<double>::min(), "double_1");
    ret &= mmkv->set("hello", "string_1");
    vector<string> v = vector<string>{"key", "value"};
    ret &= mmkv->set(v, "vector_1");
    assert(ret);

    {
        long count = mmkv->count();

        mmkv->removeValueForKey("bool_1");
        mmkv->removeValuesForKeys({"int_1", "long_1"});

        auto newCount = mmkv->count();
        assert(count == newCount + 3);
    }

    auto bValue = mmkv->getBool("bool_1");
    assert(!bValue);

    auto iValue = mmkv->getInt32("int_1");
    assert(iValue == 0);

    auto lValue = mmkv->getInt64("long_1");
    assert(lValue == 0);

    auto fValue = mmkv->getFloat("float_1");
    assert(EqualWithAccuracy(fValue, numeric_limits<float>::min(), 0.001f));

    double dValue = mmkv->getDouble("double_1");
    assert(EqualWithAccuracy(dValue, numeric_limits<double>::min(), 0.001));

    string sValue;
    ret = mmkv->getString("string_1", sValue);
    assert(ret && sValue == "hello");

    vector<string> vValue;
    ret = mmkv->getVector("vector_1", vValue);
    assert(ret && vValue == v);

    printf("test remove: passed\n");
}

int main() {
    locale::global(locale(""));
    wcout.imbue(locale(""));
    char c;
    srand((uint64_t) &c);

    string rootDir = "/tmp/mmkv";
    MMKV::initializeMMKV(rootDir);

    auto mmkv = MMKV::mmkvWithID("unit_test");
    mmkv->clearAll();

    testBool(mmkv);
    testInt32(mmkv);
    testInt64(mmkv);
    testUInt32(mmkv);
    testUInt64(mmkv);
    testFloat(mmkv);
    testDouble(mmkv);
    testString(mmkv);
    testBytes(mmkv);
    testVector(mmkv);
    testRemove(mmkv);
}
