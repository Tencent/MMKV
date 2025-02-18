//
//  TestMMKVCpp.cpp
//  MMKVDemo
//
//  Created by lingol on 2025/2/12.
//  Copyright © 2025 Lingol. All rights reserved.
//

#include "TestMMKVCpp.hpp"
#include <string>

using namespace std;
using namespace mmkv;

#define MMKVLog printf

string to_string(const std::string& str) {  return str; }

template <class T>
string to_string(const vector<T> &arr, const char* sp = ", ") {
    string str;
    for (const auto &element : arr) {
        str += to_string(element);
        str += sp;
    }
    if (!str.empty()) {
        str.erase(str.length() - strlen(sp));
    }
    return str;
}

void containerTest(MMKV* mmkv, bool decodeOnly) {
    {
        if (!decodeOnly) {
            vector<string> vec = {"Hello", "MMKV-示例", "for", "POSIX"};
            mmkv->set(vec, "string-set");
        }
        vector<string> vecResult;
        mmkv->getVector("string-set", vecResult);
        printf("string-set = %s\n", to_string(vecResult).c_str());
    }
#if __cplusplus>=202002L
    {
        if (!decodeOnly) {
            vector<bool> vec = {true, false, std::numeric_limits<bool>::min(), std::numeric_limits<bool>::max()};
            mmkv->set(vec, "bool-set");
        }
        vector<bool> vecResult;
        mmkv->getVector("bool-set", vecResult);
        printf("bool-set = %s\n", to_string(vecResult).c_str());
    }
    
    {
        if (!decodeOnly) {
            vector<int32_t> vec = {1024, 0, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()};
            mmkv->set(vec, "int32-set");
        }
        vector<int32_t> vecResult;
        mmkv->getVector("int32-set", vecResult);
        printf("int32-set = %s\n", to_string(vecResult).c_str());
    }
    
    {
        if (!decodeOnly) {
            constexpr uint32_t arr[] = {2048, 0, std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max()};
            std::span vec = arr;
            mmkv->set(vec, "uint32-set");
        }
        vector<uint32_t> vecResult;
        mmkv->getVector("uint32-set", vecResult);
        printf("uint32-set = %s\n", to_string(vecResult).c_str());
    }
    
    {
        if (!decodeOnly) {
            constexpr int64_t vec[] = {1024, 0, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()};
            mmkv->set(std::span(vec), "int64-set");
        }
        vector<int64_t> vecResult;
        mmkv->getVector("int64-set", vecResult);
        printf("int64-set = %s\n", to_string(vecResult).c_str());
    }
    
    {
        if (!decodeOnly) {
            vector<uint64_t> vec = {1024, 0, std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max()};
            mmkv->set(vec, "uint64-set");
        }
        vector<uint64_t> vecResult;
        mmkv->getVector("uint64-set", vecResult);
        printf("uint64-set = %s\n", to_string(vecResult).c_str());
    }
    
    {
        if (!decodeOnly) {
            vector<float> vec = {1024.0f, 0.0f, std::numeric_limits<float>::min(), std::numeric_limits<float>::max()};
            mmkv->set(vec, "float-set");
        }
        vector<float> vecResult;
        mmkv->getVector("float-set", vecResult);
        printf("float-set = %s\n", to_string(vecResult).c_str());
    }
    
    {
        if (!decodeOnly) {
            vector<double> vec = {1024.0, 0.0, std::numeric_limits<double>::min(), std::numeric_limits<double>::max()};
            mmkv->set(vec, "double-set");
        }
        vector<double> vecResult;
        mmkv->getVector("double-set", vecResult);
        printf("double-set = %s\n", to_string(vecResult).c_str());
        
        // Un-comment to test the functionality of set<!MMKV_SUPPORTED_VALUE_TYPE<T>>(const T& value, key)
        // mmkv->set(&vecResult, "unsupported-type");
    }
#endif // __cplusplus>=202002L
}

void functionalTest(MMKV *mmkv, bool decodeOnly) {
    if (!decodeOnly) {
        mmkv->set(true, "bool");
    }
    MMKVLog("bool = %d\n", mmkv->getBool("bool"));
    
    if (!decodeOnly) {
        mmkv->set(1024, "int32");
    }
    MMKVLog("int32 = %d\n", mmkv->getInt32("int32"));
    
    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<uint32_t>::max(), "uint32");
    }
    MMKVLog("uint32 = %u\n", mmkv->getUInt32("uint32"));
    
    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<int64_t>::min(), "int64");
    }
    MMKVLog("int64 = %lld\n", mmkv->getInt64("int64"));
    
    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<uint64_t>::max(), "uint64");
    }
    MMKVLog("uint64 = %llu\n", mmkv->getUInt64("uint64"));
    
    if (!decodeOnly) {
        mmkv->set(3.14f, "float");
    }
    MMKVLog("float = %f\n", mmkv->getFloat("float"));
    
    if (!decodeOnly) {
        mmkv->set(std::numeric_limits<double>::max(), "double");
    }
    MMKVLog("double = %f\n", mmkv->getDouble("double"));
    
    if (!decodeOnly) {
        mmkv->set("Hello, MMKV-示例 for POSIX", "raw_string");
        std::string str = "Hello, MMKV-示例 for POSIX string";
        mmkv->set(str, "string");
        mmkv->set(std::string_view(str).substr(7, 21), "string_view");
    }
    std::string result;
    mmkv->getString("raw_string", result);
    MMKVLog("raw_string = %s\n", result.c_str());
    mmkv->getString("string", result);
    MMKVLog("string = %s\n", result.c_str());
    mmkv->getString("string_view", result);
    MMKVLog("string_view = %s\n", result.c_str());
    
    containerTest(mmkv, decodeOnly);
    
    MMKVLog("allKeys: %s\n", ::to_string(mmkv->allKeys()).c_str());
    MMKVLog("count = %zu, totalSize = %zu\n", mmkv->count(), mmkv->totalSize());
    MMKVLog("containsKey[string]: %d\n", mmkv->containsKey("string"));
    
    mmkv->removeValueForKey("bool");
    MMKVLog("bool: %d\n", mmkv->getBool("bool"));
    mmkv->removeValuesForKeys({"int", "long"});
    
    mmkv->set("some string", "null string");
    result.erase();
    mmkv->getString("null string", result);
    MMKVLog("string before set null: %s\n", result.c_str());
    mmkv->set((const char *) nullptr, "null string");
    //mmkv->set("", "null string");
    result.erase();
    mmkv->getString("null string", result);
    MMKVLog("string after set null: %s, containsKey: %d\n", result.c_str(), mmkv->containsKey("null string"));
    
    //kv.sync();
    //kv.async();
    //kv.clearAll();
    mmkv->clearMemoryCache();
    MMKVLog("allKeys: %s\n", ::to_string(mmkv->allKeys()).c_str());
    MMKVLog("isFileValid[%s]: %d\n", mmkv->mmapID().c_str(), MMKV::isFileValid(mmkv->mmapID()));
}

void functionalTest(bool decodeOnly) {
    auto kv = MMKV::mmkvWithID("testCpp");
    functionalTest(kv, decodeOnly);
}

// void baseline(mmkv::MMKV *kv) {
//     return;
// }
