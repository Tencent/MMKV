// swift-tools-version:5.3
// The swift-tools-version declares the minimum version of Swift required to build this package.

/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import PackageDescription

let package = Package(
    name: "MMKV",
    platforms: [
        .iOS(.v9),
        .macOS(.v10_10),
        .watchOS(.v2),
    ],
    products: [
        // Products define the executables and libraries a package produces, and make them visible to other packages.
        .library(
            name: "MMKV Static",
            targets: ["MMKV"]),
        .library(
            name: "MMKV",
            type: .dynamic,
            targets: ["MMKV"]),
        .library(
            name: "MMKVAppExtension",
            type: .dynamic,
            targets: ["MMKV"]),
        .library(
            name: "MMKVWatchExtension",
            type: .dynamic,
            targets: ["MMKV"]),
    ],
    targets: [
        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
        // Targets can depend on other targets in this package, and on products in packages this package depends on.
        .target(
            name: "MMKVCore",
            dependencies: ["MMKVCoreOpenSSL", "MMKVCoreOpenSSLASM"],
            path: "./Core",
            exclude: ["crc32/zlib", "aes/openssl", "CMakeCache.txt", "Debug", "Release", "CMakeFiles", "CMakeLists.txt", "core.vcxproj", "core.vcxproj.user", "core.vcxproj.filters", "core.dir", "cmake_install.cmake",
                "MMKVMetaInfo.hpp", "PBEncodeItem.hpp", "include/MMKVCore/ScopedLock.hpp", // workaround for SPM's bug
                //"aes/openssl/openssl_aes_core.cpp", "aes/openssl/openssl_md5_one.cpp", "aes/openssl/openssl_cfb128.cpp", "aes/openssl/openssl_md5_dgst.cpp", // workaround for SPM's bug
            ],
            publicHeadersPath: "include",
            cSettings : [
                .define("DEBUG", to: "1", .when(configuration: .debug)),
                .define("NDEBUG", to: "1", .when(configuration: .release)),
                .headerSearchPath("include/MMKVCore"),
            ],
            cxxSettings: [
                .headerSearchPath("."),
                .unsafeFlags(["-x", "objective-c++", "-fno-objc-arc"]),
            ]
        ),
        .target(
            name: "MMKVCoreOpenSSL",
            path: "./Core/aes/openssl",
            exclude: ["openssl_aes-armv4.S", "openssl_aesv8-armx.S"],
            //publicHeadersPath: ".",
            cSettings : [
                .define("DEBUG", to: "1", .when(configuration: .debug)),
                .define("NDEBUG", to: "1", .when(configuration: .release)),
            ],
            cxxSettings: [
                .headerSearchPath("."),
                .headerSearchPath("../../include/MMKVCore"),
                .unsafeFlags(["-x", "objective-c++", "-fno-objc-arc"]),
            ]
        ),
        .target(
            name: "MMKVCoreOpenSSLASM",
            path: "./Core/aes/openssl",
            exclude: ["openssl_aes_core.cpp", "openssl_md5_one.cpp", "openssl_cfb128.cpp", "openssl_md5_dgst.cpp"], // workaround for SPM's bug when setting publicHeadersPath the same as path
            sources: ["openssl_aes-armv4.S", "openssl_aesv8-armx.S"],
            publicHeadersPath: ".",
            cSettings : [
                .define("DEBUG", to: "1", .when(configuration: .debug)),
                .define("NDEBUG", to: "1", .when(configuration: .release)),
                .headerSearchPath("../../include/MMKVCore"),
            ]
        ),
        .target(
            name: "MMKV",
            dependencies: ["MMKVCore"],
            path: "./iOS/MMKV/MMKV",
            exclude: ["Resources"],
            publicHeadersPath: ".",
            cSettings : [
                .define("DEBUG", to: "1", .when(configuration: .debug)),
                .define("NDEBUG", to: "1", .when(configuration: .release)),
                .define("MMKV_IOS_EXTENSION", .when(platforms: [.watchOS])),
            ],
            cxxSettings: [
                .headerSearchPath("."),
                .unsafeFlags(["-x", "objective-c++", "-fno-objc-arc"]),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
                .linkedFramework("UIKit", .when(platforms: [.iOS])),
                .linkedLibrary("z"),
                //.unsafeFlags(["-ObjC"]),
            ]
         ),
    ],
    cLanguageStandard: .gnu11,
    cxxLanguageStandard: .gnucxx1z
)
