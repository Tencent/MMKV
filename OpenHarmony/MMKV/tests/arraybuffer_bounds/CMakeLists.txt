cmake_minimum_required(VERSION 3.16)
project(mmkv_ohos_arraybuffer_bounds_tests LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/main/cpp")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native_callbacks.inc"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_callbacks.py"
            "${CMAKE_CURRENT_BINARY_DIR}/native_callbacks.inc"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/prepare_callbacks.py" "${BRIDGE_DIR}/native_bridge.cpp"
    VERBATIM)
add_executable(arraybuffer_bounds_tests tests.cpp "${CMAKE_CURRENT_BINARY_DIR}/native_callbacks.inc")
target_compile_features(arraybuffer_bounds_tests PRIVATE cxx_std_20)
target_include_directories(arraybuffer_bounds_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}" "${BRIDGE_DIR}")
target_compile_options(arraybuffer_bounds_tests PRIVATE -Wall -Wextra -Werror)
option(MMKV_TEST_SANITIZERS "Use AddressSanitizer and UndefinedBehaviorSanitizer" ON)
if(MMKV_TEST_SANITIZERS)
    target_compile_options(arraybuffer_bounds_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(arraybuffer_bounds_tests PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME arraybuffer_bounds_tests COMMAND arraybuffer_bounds_tests)
