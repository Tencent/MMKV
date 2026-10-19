cmake_minimum_required(VERSION 3.10)
project(restore_mapped_test CXX)

if(NOT WIN32)
    message(FATAL_ERROR "restore_mapped_test is a Windows-only test")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the same static core library the production build uses. Core/CMakeLists.txt exports
# its include directory, so the test includes <MMKV/MMKV.h> like any other consumer.
add_subdirectory(../../../Core ${CMAKE_CURRENT_BINARY_DIR}/core)

add_executable(restore_mapped_test main.cpp)
target_link_libraries(restore_mapped_test PRIVATE core)

enable_testing()
add_test(NAME restore_mapped_test COMMAND restore_mapped_test)
