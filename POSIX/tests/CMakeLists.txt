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

# This regression uses GNU-compatible linker wrapping of time().
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(TestKeyExpiration TestKeyExpiration.cpp)
    target_link_libraries(TestKeyExpiration core pthread "-Wl,--wrap=time")
    set_target_properties(TestKeyExpiration PROPERTIES CXX_STANDARD 20)
    add_test(NAME TestKeyExpiration COMMAND TestKeyExpiration)
endif()
