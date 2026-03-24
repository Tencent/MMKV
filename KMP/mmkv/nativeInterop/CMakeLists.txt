#
# Tencent is pleased to support the open source community by making
# MMKV available.
#
# Copyright (C) 2026 THL A29 Limited, a Tencent company.
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

cmake_minimum_required(VERSION 3.14)
project(mmkv-kmp)

# Set the version of MMKV to use.
set(MMKV_VERSION "v2.4.0")

# Auto-detect: if running inside the MMKV monorepo, use local source;
# otherwise fetch from GitHub (e.g. when built as a standalone package).
set(MMKV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

include(FetchContent)
IF(EXISTS "${MMKV_ROOT}/Core/cbridge/MMKVBridge.h")
    message(STATUS "Using local MMKV Core from ${MMKV_ROOT}/Core")
    FetchContent_Declare(mmkv
      SOURCE_DIR "${MMKV_ROOT}"
      SOURCE_SUBDIR "Core"
    )
ELSE()
    message(STATUS "Fetching MMKV ${MMKV_VERSION} from GitHub")
    FetchContent_Declare(mmkv
      GIT_REPOSITORY "https://github.com/Tencent/MMKV.git"
      GIT_TAG ${MMKV_VERSION}
      SOURCE_SUBDIR "Core"
    )
ENDIF()
FetchContent_MakeAvailable(mmkv)
FetchContent_GetProperties(mmkv SOURCE_DIR MMKV_SOURCE_DIR)

# Write the resolved cbridge header path so Gradle cinterop can find it.
file(WRITE ${CMAKE_BINARY_DIR}/cbridge_include_dir.txt "${MMKV_SOURCE_DIR}/Core/cbridge")

# The Core CMakeLists.txt builds the 'core' static library which
# includes all Core sources plus cbridge/MMKVBridge.cpp.
# We rename it to libmmkv-kmp.a for cinterop linking.

set(OUTPUT_LIB ${CMAKE_BINARY_DIR}/libmmkv-kmp.a)

# Copy libcore.a -> libmmkv-kmp.a
add_custom_command(
    OUTPUT ${OUTPUT_LIB}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:core> ${OUTPUT_LIB}
    DEPENDS core
    COMMENT "Copying libcore.a to libmmkv-kmp.a"
)

add_custom_target(mmkv-kmp ALL DEPENDS ${OUTPUT_LIB})

# Also build a shared library for JVM desktop (JNA).
# Use whole-archive to re-export all C bridge symbols from core.
file(WRITE ${CMAKE_BINARY_DIR}/empty.cpp "")
add_library(mmkv-kmp-shared SHARED ${CMAKE_BINARY_DIR}/empty.cpp)
set_target_properties(mmkv-kmp-shared PROPERTIES
    OUTPUT_NAME "mmkv-kmp"
    CXX_STANDARD 20
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

IF(APPLE)
    target_link_libraries(mmkv-kmp-shared -Wl,-force_load core)
ELSE()
    target_link_libraries(mmkv-kmp-shared -Wl,--whole-archive core -Wl,--no-whole-archive)
ENDIF()

find_library(zlib z)
IF(zlib)
    target_link_libraries(mmkv-kmp-shared ${zlib})
ENDIF()
