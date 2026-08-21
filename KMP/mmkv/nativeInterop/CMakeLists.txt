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

# Set the version of MMKV to use. Can be overridden by Gradle via
# -DMMKV_VERSION=v<x.y.z>. Defaults to the in-repo value when run standalone.
set(MMKV_VERSION "v2.4.2" CACHE STRING "MMKV version to fetch when built outside the monorepo")
set(MMKV_GIT_REPOSITORY "https://github.com/Tencent/MMKV.git" CACHE STRING "MMKV git repository to fetch when built outside the monorepo")
set(MMKV_GIT_TAG "${MMKV_VERSION}" CACHE STRING "MMKV git tag, branch, or commit to fetch when built outside the monorepo")

# Auto-detect: if running inside the MMKV monorepo, use local source;
# otherwise fetch from GitHub (e.g. when built as a standalone package).
# Set MMKV_FORCE_FETCH=ON to exercise the fetch path locally.
set(MMKV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH "Local MMKV checkout root")
set(MMKV_FORCE_FETCH OFF CACHE BOOL "Fetch MMKV even when a local checkout is present")

include(FetchContent)
IF(EXISTS "${MMKV_ROOT}/Core/cbridge/MMKVBridge.h" AND NOT MMKV_FORCE_FETCH)
    message(STATUS "Using local MMKV Core from ${MMKV_ROOT}/Core")
    FetchContent_Declare(mmkv
      SOURCE_DIR "${MMKV_ROOT}"
      SOURCE_SUBDIR "Core"
    )
ELSE()
    message(STATUS "Fetching MMKV ${MMKV_GIT_TAG} from ${MMKV_GIT_REPOSITORY}")
    FetchContent_Declare(mmkv
      GIT_REPOSITORY "${MMKV_GIT_REPOSITORY}"
      GIT_TAG "${MMKV_GIT_TAG}"
      GIT_SUBMODULES ""
      SOURCE_SUBDIR "Core"
    )
ENDIF()
FetchContent_MakeAvailable(mmkv)
FetchContent_GetProperties(mmkv SOURCE_DIR MMKV_SOURCE_DIR)

# Copy the C bridge header into a stable, build-local include directory.
# Gradle can configure cinterop with this path before FetchContent has resolved
# the source checkout, which also makes standalone KMP source builds work.
set(MMKV_CBRIDGE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${MMKV_CBRIDGE_INCLUDE_DIR}")
configure_file(
    "${MMKV_SOURCE_DIR}/Core/cbridge/MMKVBridge.h"
    "${MMKV_CBRIDGE_INCLUDE_DIR}/MMKVBridge.h"
    COPYONLY
)
file(WRITE "${CMAKE_BINARY_DIR}/cbridge_include_dir.txt" "${MMKV_CBRIDGE_INCLUDE_DIR}")

# The Core CMakeLists.txt builds the 'core' static library which includes all
# Core sources plus cbridge/MMKVBridge.cpp. Copy it to the name expected by
# cinterop's staticLibraries entry.
set(OUTPUT_LIB ${CMAKE_BINARY_DIR}/libmmkv-kmp.a)

add_custom_command(
    OUTPUT ${OUTPUT_LIB}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:core> ${OUTPUT_LIB}
    DEPENDS core
    COMMENT "Copying libcore.a to libmmkv-kmp.a"
)

add_custom_target(mmkv-kmp ALL DEPENDS ${OUTPUT_LIB})
