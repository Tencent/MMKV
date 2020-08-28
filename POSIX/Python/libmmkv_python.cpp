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
#include <pybind11/pybind11.h>

using namespace mmkv;
using namespace std;
namespace py = pybind11;

static MMBuffer pyBytes2MMBuffer(const py::bytes &bytes) {
    char *buffer = nullptr;
    ssize_t length = 0;
    if (PYBIND11_BYTES_AS_STRING_AND_SIZE(bytes.ptr(), &buffer, &length) == 0) {
        return MMBuffer(buffer, length, MMBufferNoCopy);
    }
    return MMBuffer(0);
}

PYBIND11_MODULE(mmkv, m) {
    m.doc() = "An efficient, small mobile key-value storage framework developed by WeChat Team.";

    py::enum_<MMKVMode>(m, "MMKVMode")
        .value("SingleProcess", MMKVMode::MMKV_SINGLE_PROCESS)
        .value("MultiProcess", MMKVMode::MMKV_MULTI_PROCESS)
        .export_values();

    py::enum_<MMKVLogLevel>(m, "MMKVLogLevel")
        .value("LogNone", MMKVLogLevel::MMKVLogNone)
        .value("LogDebug", MMKVLogLevel::MMKVLogDebug)
        .value("LogInfo", MMKVLogLevel::MMKVLogInfo)
        .value("LogWarning", MMKVLogLevel::MMKVLogWarning)
        .value("LogError", MMKVLogLevel::MMKVLogError)
        .export_values();

    py::class_<MMKV, unique_ptr<MMKV, py::nodelete>> clsMMKV(m, "MMKV");
    //        .def(py::init(&MMKV::mmkvWithID), // TODO: not working
    //             py::arg("mmapID"),
    //             py::arg("mode") = MMKV_SINGLE_PROCESS,
    //             py::arg("cryptKey") = (string*) nullptr,
    //             py::arg("rootDir") = (string*) nullptr)
    clsMMKV.def(py::init([](const string &mmapID, MMKVMode mode, const string &cryptKey, const string &rootDir) {
                    string *cryptKeyPtr = (cryptKey.length() > 0) ? (string *) &cryptKey : nullptr;
                    string *rootDirPtr = (rootDir.length() > 0) ? (string *) &rootDir : nullptr;
                    return MMKV::mmkvWithID(mmapID, mode, cryptKeyPtr, rootDirPtr);
                }),
                py::arg("mmapID"), py::arg("mode") = MMKV_SINGLE_PROCESS, py::arg("cryptKey") = string(),
                py::arg("rootDir") = string());

    clsMMKV.def_static("initializeMMKV", &MMKV::initializeMMKV, py::arg("rootDir"), py::arg("logLevel") = MMKVLogNone);

    clsMMKV.def_static(
        "defaultMMKV",
        [](MMKVMode mode, const string &cryptKey) {
            string *cryptKeyPtr = (cryptKey.length() > 0) ? (string *) &cryptKey : nullptr;
            return MMKV::defaultMMKV(mode, cryptKeyPtr);
        },
        py::arg("mode") = MMKV_SINGLE_PROCESS, py::arg("cryptKey") = string());

    clsMMKV.def("mmapID", &MMKV::mmapID);
    clsMMKV.def_readonly("isInterProcess", &MMKV::m_isInterProcess);

    clsMMKV.def("cryptKey", &MMKV::cryptKey);
    clsMMKV.def("reKey", &MMKV::reKey, py::arg("newCryptKey"));
    clsMMKV.def("checkReSetCryptKey", &MMKV::checkReSetCryptKey, py::arg("newCryptKey"));

    // TODO: Doesn't work, why?
    // clsMMKV.def("set", py::overload_cast<bool, const string&>(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("setBool", (bool (MMKV::*)(bool, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("set", (bool (MMKV::*)(int32_t, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("set", (bool (MMKV::*)(uint32_t, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("set", (bool (MMKV::*)(int64_t, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("set", (bool (MMKV::*)(uint64_t, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("set", (bool (MMKV::*)(float, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("set", (bool (MMKV::*)(double, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
    //clsMMKV.def("set", (bool (MMKV::*)(const char*, const string&))(&MMKV::set), py::arg("value"), py::arg("key"));
    clsMMKV.def("set", (bool (MMKV::*)(const string &, const string &))(&MMKV::set), py::arg("value"), py::arg("key"));
#if PY_MAJOR_VERSION >= 3
    clsMMKV.def(
        "set", [](MMKV &kv, const py::bytes &value, const string &key) { return kv.set(pyBytes2MMBuffer(value), key); },
        py::arg("value"), py::arg("key"));
#endif

    clsMMKV.def("getBool", &MMKV::getBool, py::arg("key"), py::arg("defaultValue") = false);
    clsMMKV.def("getInt", &MMKV::getInt32, py::arg("key"), py::arg("defaultValue") = 0);
    clsMMKV.def("getUInt", &MMKV::getUInt32, py::arg("key"), py::arg("defaultValue") = 0);
    clsMMKV.def("getLongInt", &MMKV::getInt64, py::arg("key"), py::arg("defaultValue") = 0);
    clsMMKV.def("getLongUInt", &MMKV::getUInt64, py::arg("key"), py::arg("defaultValue") = 0);
    clsMMKV.def("getFloat", &MMKV::getFloat, py::arg("key"), py::arg("defaultValue") = 0);
    clsMMKV.def("getDouble", &MMKV::getDouble, py::arg("key"), py::arg("defaultValue") = 0);
    clsMMKV.def(
        "getStr",
        [](MMKV &kv, const string &key, const string &defaultValue) {
            string result;
            if (kv.getString(key, result)) {
                return result;
            }
            return defaultValue;
        },
        py::arg("key"), py::arg("defaultValue") = string());
    clsMMKV.def(
        "getBytes",
        [](MMKV &kv, const string &key, const py::bytes &defaultValue) {
            MMBuffer result = kv.getBytes(key);
            if (result.length() > 0) {
                return py::bytes((const char *) result.getPtr(), result.length());
            }
            return defaultValue;
        },
        py::arg("key"), py::arg("defaultValue") = py::bytes());
}
