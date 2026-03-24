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

include(FetchContent)
FetchContent_Declare(mmkv
  GIT_REPOSITORY "https://github.com/Tencent/MMKV.git"
  GIT_TAG ${MMKV_VERSION}
  SOURCE_SUBDIR "Core"
)
FetchContent_MakeAvailable(mmkv)

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
