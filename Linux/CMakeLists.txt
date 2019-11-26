# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             mmkv

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             #src/main/src/native-bridge.cpp
             src/MMKV.h
             src/MMKV.cpp
             src/MMKVLog.h
             src/MMKVLog.cpp
             src/CodedInputData.h
             src/CodedInputData.cpp
             src/CodedOutputData.h
             src/CodedOutputData.cpp
             src/PBUtility.h
             src/PBUtility.cpp
             src/MiniPBCoder.h
             src/MiniPBCoder.cpp
             src/MMBuffer.h
             src/MMBuffer.cpp
             src/InterProcessLock.h
             src/InterProcessLock.cpp
             src/MmapedFile.h
             src/MmapedFile.cpp
             src/ThreadLock.h
             src/ThreadLock.cpp
             src/MMKVMetaInfo.hpp
             src/aes/AESCrypt.h
             src/aes/AESCrypt.cpp
             src/aes/openssl/aes.h
             src/aes/openssl/aes_cfb.c
             src/aes/openssl/aes_core.c
             src/aes/openssl/aes_locl.h
             src/aes/openssl/cfb128.c
             src/aes/openssl/modes.h
             src/aes/openssl/opensslconf.h
             src/aes/openssl/md5_dgst.c
             src/aes/openssl/md5_locl.h
             src/aes/openssl/md5_one.c
             src/aes/openssl/md5.h
             src/aes/openssl/md32_common.h
             )

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

find_package(ZLIB)

set_target_properties(mmkv PROPERTIES
            CXX_STANDARD 17
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
                       z )
