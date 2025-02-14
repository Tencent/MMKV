# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(OpenHarmony)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(MODULE_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

# mmkv/OpenHarmony/entry/oh_modules/@tencent/mmkv
set(MMKV_MODULE_PATH ${MODULE_ROOT_PATH}/oh_modules/@tencent/mmkv)
# mmkv/OpenHarmony/entry/oh_modules/@tencent/mmkv/include
set(MMKV_INCLUDE_PATH ${MMKV_MODULE_PATH}/include)
# mmkv/OpenHarmony/entry/oh_modules/@tencent/mmkv/libs/arm64-v8a
set(MMKV_LIB_PATH ${MMKV_MODULE_PATH}/libs/${OHOS_ARCH})
# mmkv/OpenHarmony/entry/oh_modules/@tencent/mmkv/build/default/intermediates/cmake/default/obj/arm64-v8a
set(MMKV_EMBED_LIB_PATH ${MMKV_MODULE_PATH}/build/default/intermediates/cmake/default/obj/${OHOS_ARCH})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

add_library(entry SHARED napi_init.cpp)
target_link_libraries(entry PUBLIC libace_napi.z.so libhilog_ndk.z.so)

# setup mmkv include path
target_include_directories(entry PRIVATE ${MMKV_INCLUDE_PATH})
# setup mmkv lib path
add_library(mmkv SHARED IMPORTED)
if (EXISTS ${MMKV_LIB_PATH}/libmmkv.so)
    set_target_properties(mmkv PROPERTIES IMPORTED_LOCATION ${MMKV_LIB_PATH}/libmmkv.so)
elseif (EXISTS ${MMKV_EMBED_LIB_PATH}/libmmkv.so)
    set_target_properties(mmkv PROPERTIES IMPORTED_LOCATION ${MMKV_EMBED_LIB_PATH}/libmmkv.so)
else()
    message(AUTHOR_WARNING "Can't find libmmkv.so in @tencent/mmkv module:${MMKV_MODULE_PATH}, check if \"ohpm install @tencent/mmkv\" fail!")
    set_target_properties(mmkv PROPERTIES IMPORTED_LOCATION ${MMKV_EMBED_LIB_PATH}/libmmkv.so)
endif()
target_link_libraries(entry PRIVATE mmkv)

## Function to print all CMake variables
#function(print_all_variables)
#    message(STATUS "Printing all CMake variables:")
#    get_cmake_property(_variableNames VARIABLES)
#    foreach (_variableName ${_variableNames})
#        message(STATUS "${_variableName} = ${${_variableName}}")
#    endforeach()
#endfunction()
#
## Call the function to print all variables
#print_all_variables()
#message(FATAL_ERROR "Halt!")

