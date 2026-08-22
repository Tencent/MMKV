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
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

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
             SHARED

             # Provides a relative path to your source file(s).
             libmmkv.cpp
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
            CXX_STANDARD 20
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
                       core
        pthread
        )

# Try include GNUInstallDirs
include(GNUInstallDirs OPTIONAL)

# If succeeded, then continue
if(DEFINED CMAKE_INSTALL_LIBDIR)
        add_executable( # Sets the name of the installer binary
                mmkv_install
                EXCLUDE_FROM_ALL # Do not build by default

                # Provides a relative path to your source file(s).
                libmmkv.cpp
        )

        target_include_directories(mmkv_install PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/include )

        target_compile_definitions(mmkv_install PRIVATE
                # Sets compile definition
                POSIX_INSTALLER_MODE="ON"
                INCLUDEDIR="${CMAKE_CURRENT_SOURCE_DIR}/../../Core/include/MMKV"
                LIBRARY="$<TARGET_FILE:mmkv>"
                PREFIX="${CMAKE_INSTALL_PREFIX}"
                LIBDIR="${CMAKE_INSTALL_LIBDIR}"
                INCLUDE="${CMAKE_INSTALL_INCLUDEDIR}"
        )

        # Get the platform-specific executable suffix (e.g. ".exe" on Windows, "" on Linux/macOS)
        get_target_property(EXE_SUFFIX mmkv_install SUFFIX)
        if(NOT EXE_SUFFIX)
                set(EXE_SUFFIX "${CMAKE_EXECUTABLE_SUFFIX}")
        endif()

        install(CODE "
        # Force CMake to build the installer binary first
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build \"${CMAKE_BINARY_DIR}\" --target mmkv_install
            COMMAND_ERROR_IS_FATAL ANY
        )

        # Run the newly built installer binary
        execute_process(
            COMMAND \"${CMAKE_BINARY_DIR}/src/mmkv_install${EXE_SUFFIX}\"
            COMMAND_ERROR_IS_FATAL ANY
        )
        ")
endif()

