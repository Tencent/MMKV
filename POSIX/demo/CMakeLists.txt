#
# Tencent is pleased to support the open source community by making
# MMKV available.
#
# Copyright (C) 2019 THL A29 Limited, a Tencent company.
# All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use
# this file except in compliance with the License. You may obtain a copy of
# the License at
#
#       https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.10.0)

project(demo)

IF(APPLE)
    add_compile_definitions(FORCE_POSIX)
ENDIF()

add_executable(demo
        demo.cpp)
target_link_libraries(demo
        mmkv)
set_target_properties(demo PROPERTIES
        CXX_STANDARD 11
        )

add_executable(process
        process.cpp)
target_link_libraries(process
        mmkv)
set_target_properties(process PROPERTIES
        CXX_STANDARD 11
        )

add_executable(TestInterProcessLock
        TestInterProcessLock.cpp)
target_link_libraries(TestInterProcessLock
        mmkv)
set_target_properties(TestInterProcessLock PROPERTIES
        CXX_STANDARD 11
        )

add_executable(UnitTest
        UnitTest.cpp)
target_link_libraries(UnitTest
        mmkv)
set_target_properties(UnitTest PROPERTIES
        CXX_STANDARD 11
        )

add_dependencies(demo
        process
        TestInterProcessLock
        UnitTest)

