# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(mmkv)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(../../../../../Core Core)

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(mmkv SHARED native_bridge.cpp)

set_target_properties(mmkv PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        )

target_link_libraries(mmkv PUBLIC
    libace_napi.z.so
    libhilog_ndk.z.so
    core)