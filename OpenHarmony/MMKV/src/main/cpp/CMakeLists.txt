# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(mmkv)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
# github/MMKV
set(REPO_ROOT_PATH ${NATIVERENDER_ROOT_PATH}/../../../../..)
# github/MMKV/OpenHarmony/MMKV
set(MODULE_ROOT_PATH ${NATIVERENDER_ROOT_PATH}/../../..)

add_subdirectory(${REPO_ROOT_PATH}/Core Core)

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(mmkv SHARED
 native_bridge.cpp
 flutter-bridge.cpp)

set_target_properties(mmkv PROPERTIES
        CXX_STANDARD 20
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        )

target_link_libraries(mmkv PUBLIC
    libace_napi.z.so
    libhilog_ndk.z.so
    core)

#file(REMOVE_RECURSE ${MODULE_ROOT_PATH}/include)

message(STATUS "copying from ${REPO_ROOT_PATH}/Core/include to ${MODULE_ROOT_PATH}")
file(COPY ${REPO_ROOT_PATH}/Core/include DESTINATION ${MODULE_ROOT_PATH}
    PATTERN "MMKVLog.h" EXCLUDE
    PATTERN "MemoryFile.h" EXCLUDE
)

#message(FATAL_ERROR "Halt.")
