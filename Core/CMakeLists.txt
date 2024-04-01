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

cmake_minimum_required(VERSION 3.10.0)

IF(APPLE)
    # tell ranlib to ignore empty compilation units
    SET(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    SET(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    # prevents ar from invoking ranlib, let CMake do it
    SET(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> qc -S <TARGET> <LINK_FLAGS> <OBJECTS>")
    SET(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qc -S <TARGET> <LINK_FLAGS> <OBJECTS>")

    add_compile_definitions(FORCE_POSIX)
ENDIF()

set(can_use_assembler TRUE)
enable_language(ASM)
IF("${ANDROID_ABI}" STREQUAL "arm64-v8a")
    SET(ASM_OPTIONS "-x assembler-with-cpp")
    SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv8+crypto -D__ANDROID__")
ELSEIF("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
    SET(ASM_OPTIONS "-x assembler-with-cpp")
    SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv7a -D__ANDROID__")
ELSEIF("${ANDROID_ABI}" STREQUAL "armeabi")
    SET(ASM_OPTIONS "-x assembler-with-cpp")
    SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv5 -D__ANDROID__")
ENDIF()

#include(CMakePrintHelpers)
#cmake_print_variables(CMAKE_SYSTEM_PROCESSOR OHOS OHOS_ARCH CFLAGS ASM_OPTIONS CMAKE_ASM_FLAGS)

IF(OHOS)
    IF("${OHOS_ARCH}" STREQUAL "arm64-v8a")
        SET(ASM_OPTIONS "-x assembler-with-cpp")
        SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv8+crypto -D__MUSL__")
    ENDIF()
ELSEIF(UNIX AND (NOT APPLE))
    IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
        SET(ASM_OPTIONS "-x assembler-with-cpp")
        SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv8-a+crypto")
    ENDIF()
ENDIF()


project(core)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library(core

        # Sets the library as a shared library.
        STATIC

        # Provides a relative path to your source file(s).
        MMKV.h
        MMKV.cpp
        MMKV_Android.cpp
        MMKV_IO.h
        MMKV_IO.cpp
        MMKV_OSX.cpp
        MMKVLog.h
        MMKVLog.cpp
        MMKVLog_Android.cpp
        CodedInputData.h
        CodedInputData.cpp
        CodedInputData_OSX.cpp
        CodedInputDataCrypt.h
        CodedInputDataCrypt.cpp
        CodedInputDataCrypt_OSX.cpp
        CodedOutputData.h
        CodedOutputData.cpp
        KeyValueHolder.h
        KeyValueHolder.cpp
        PBUtility.h
        PBUtility.cpp
        MiniPBCoder.h
        MiniPBCoder.cpp
        MiniPBCoder_OSX.cpp
        MMBuffer.h
        MMBuffer.cpp
        InterProcessLock.h
        InterProcessLock.cpp
        InterProcessLock_Win32.cpp
        InterProcessLock_Android.cpp
        MemoryFile.h
        MemoryFile.cpp
        MemoryFile_Android.cpp
        MemoryFile_Linux.cpp
        MemoryFile_Win32.cpp
        MemoryFile_OSX.cpp
        ThreadLock.h
        ThreadLock.cpp
        ThreadLock_Win32.cpp
        MMKVMetaInfo.hpp
        aes/AESCrypt.h
        aes/AESCrypt.cpp
        aes/openssl/openssl_aes.h
        aes/openssl/openssl_aes_core.cpp
        aes/openssl/openssl_aes_locl.h
        aes/openssl/openssl_cfb128.cpp
        aes/openssl/openssl_opensslconf.h
        aes/openssl/openssl_md5_dgst.cpp
        aes/openssl/openssl_md5_locl.h
        aes/openssl/openssl_md5_one.cpp
        aes/openssl/openssl_md5.h
        aes/openssl/openssl_md32_common.h
        aes/openssl/openssl_arm_arch.h
        crc32/Checksum.h
        crc32/crc32_armv8.cpp
        crc32/zlib/zconf.h
        crc32/zlib/zutil.h
        crc32/zlib/crc32.h
        crc32/zlib/crc32.cpp
        MMKVPredef.h
        )

IF (MSVC OR (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"))
    # .S files is not supported by MSVC.
    # x86_64 asm not supported in OHOS
ELSE()
    target_sources(core PRIVATE
        aes/openssl/openssl_aesv8-armx.S
        aes/openssl/openssl_aes-armv4.S)
ENDIF()

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

IF (WIN32)
    # MMKV can be used only with Unicode on Windows.
    target_compile_definitions(core PUBLIC UNICODE)
    target_compile_definitions(core PUBLIC _UNICODE)
ENDIF()

set_target_properties(core PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        )

find_library(zlib
        z
        )

IF (NOT zlib)
    target_compile_definitions(core PUBLIC MMKV_EMBED_ZLIB=1)
ELSE()
    target_link_libraries(core ${zlib})
ENDIF()

