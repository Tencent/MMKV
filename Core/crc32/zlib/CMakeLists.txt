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

project(z)

IF(APPLE)
    add_compile_definitions(FORCE_POSIX)
ENDIF()


# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             z

             # Sets the library as a shared library.
             STATIC

             # Provides a relative path to your source file(s).
             crc32.h
             crc32.cpp
             zconf.h
             zutil.h
        )


set_target_properties(z PROPERTIES
            CXX_STANDARD 17
            CXX_EXTENSIONS OFF
            )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.


