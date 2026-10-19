cmake_minimum_required(VERSION 3.10)
project(copy_large_file_test LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "copy_large_file_test requires Windows")
endif()

add_subdirectory(../../../Core ${CMAKE_CURRENT_BINARY_DIR}/core)
add_executable(copy_large_file_test main.cpp)
target_include_directories(copy_large_file_test PRIVATE ../../../Core)
target_link_libraries(copy_large_file_test PRIVATE core)
target_compile_features(copy_large_file_test PRIVATE cxx_std_17)

enable_testing()
add_test(NAME copy_large_file_test COMMAND copy_large_file_test)
