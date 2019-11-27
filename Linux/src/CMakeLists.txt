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

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

project(mmkv)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             mmkv

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             MMKV.h
             MMKV.cpp
             MMKVLog.h
             MMKVLog.cpp
             CodedInputData.h
             CodedInputData.cpp
             CodedOutputData.h
             CodedOutputData.cpp
             PBUtility.h
             PBUtility.cpp
             MiniPBCoder.h
             MiniPBCoder.cpp
             MMBuffer.h
             MMBuffer.cpp
             InterProcessLock.h
             InterProcessLock.cpp
             MmapedFile.h
             MmapedFile.cpp
             ThreadLock.h
             ThreadLock.cpp
             MMKVMetaInfo.hpp
             native-bridge.h
             native-bridge.cpp
             aes/AESCrypt.h
             aes/AESCrypt.cpp
             aes/openssl/aes.h
             aes/openssl/aes_cfb.c
             aes/openssl/aes_core.c
             aes/openssl/aes_locl.h
             aes/openssl/cfb128.c
             aes/openssl/modes.h
             aes/openssl/opensslconf.h
             aes/openssl/md5_dgst.c
             aes/openssl/md5_locl.h
             aes/openssl/md5_one.c
             aes/openssl/md5.h
             aes/openssl/md32_common.h
             crc32/Checksum.h
             crc32/zlib/zconf.h
             crc32/zlib/zutil.h
             crc32/zlib/crc32.h
             crc32/zlib/crc32.cpp
             )

target_include_directories(mmkv PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

#find_library( # Sets the name of the path variable.
  #              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              #            log
              #)

set_target_properties(mmkv PROPERTIES
            CXX_STANDARD 17
            CXX_EXTENSIONS OFF
            )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       mmkv

                       # Links the target library to the log library
                       # included in the NDK.
                       #          ${log-lib}
        pthread
        dl)
