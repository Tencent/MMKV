#
# Tencent is pleased to support the open source community by making
# MMKV available.
#
# Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.0)

project(mmkv)

IF(APPLE)
    add_compile_definitions(FORCE_POSIX)
ENDIF()

add_subdirectory(../../Core Core)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             mmkv

             # Sets the library as a shared library.
             STATIC

             # Provides a relative path to your source file(s).
             golang-bridge.h
             golang-bridge.cpp
        )

target_include_directories(mmkv PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(mmkv PROPERTIES
            CXX_STANDARD 17
            CXX_EXTENSIONS OFF
            )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( mmkv
                       core
                       pthread
        )

# Install source file & library to tencent.com/mmkv dir

install(FILES mmkv.go callback.go go.mod mmkv_test.go golang-bridge.h golang-bridge.cpp
  DESTINATION "tencent.com/mmkv")

install(FILES libmmkv.a Core/libcore.a
  DESTINATION "tencent.com/mmkv/lib")

