// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "MMKV",
    platforms: [
        .iOS(.v13), // Bumped to reasonable modern minimums
        .macOS(.v10_15),
        .watchOS(.v6),
        .tvOS(.v13),
        .visionOS(.v1)
    ],
    products: [
        .library(name: "MMKV", targets: ["MMKV"]),
        .library(name: "MMKVAppExtension", type: .dynamic, targets: ["MMKVAppExtension"]),
        .library(name: "MMKVAppExtension-static", type: .static, targets: ["MMKVAppExtension"]),
        .library(name: "MMKVCore", type: .static, targets: ["MMKVCore"]),
    ],
    targets: [
        // The Core C++ Logic
        .target(
            name: "MMKVCore",
            dependencies: ["MMKVCoreOpenSSL"], // ASM target linked transitively
            path: "Core",
            exclude: [
                "aes/openssl", // Moved to separate target
                "crc32/zlib", // Assuming system zlib
                "CMakeFiles",
                "CMakeLists.txt",
            ],
            publicHeadersPath: "fakeinclude", // Cleanly exposes headers in Core/include
            cSettings: [
                .define("NDEBUG", .when(configuration: .release))
            ],
            cxxSettings: [
                .headerSearchPath("fakeinclude/MMKVCore"), // Allow internal imports like #include "MMKV.h"
                .headerSearchPath("."),
                .unsafeFlags(["-x", "objective-c++", "-fno-objc-arc"]) 
            ],
            linkerSettings: [
                .linkedLibrary("z"),
                .linkedLibrary("c++")
            ]
        ),

        // OpenSSL C++ Implementation
        .target(
            name: "MMKVCoreOpenSSL",
            // dependencies: ["MMKVCoreOpenSSLASM"], // Link the assembly code
            path: "Core/aes/openssl",
            publicHeadersPath: ".", // Expose these headers to Core
            cxxSettings: [
                // .headerSearchPath("../../include/MMKVCore"),
                .unsafeFlags(["-fno-objc-arc"])
            ]
        ),

        // The iOS/ObjC Wrapper (The main target users import)
        .target(
            name: "MMKV",
            dependencies: ["MMKVCore"],
            path: "iOS/MMKV/MMKV",
            exclude: [
                "MMKVAppExtension",
                "MMKVWatchExtension",
            ],
            resources: [
                // NEW: Required for App Store submission in 2025
                .copy("Resources/PrivacyInfo.xcprivacy") 
            ],
            publicHeadersPath: "fakeinclude",
            cSettings: [
                .define("MMKV_IOS_EXTENSION", .when(platforms: [.watchOS]))
            ],
            cxxSettings: [
                .headerSearchPath("."),
                // .headerSearchPath("../../../Core/include"),
                // Modern Swift 6.2 allows this without blocking dependencies
                .unsafeFlags(["-x", "objective-c++", "-fno-objc-arc"])
            ],
            linkerSettings: [
                .linkedLibrary("z"),
                .linkedLibrary("c++")
            ]
        ),
        // for App Extension
        .target(
            name: "MMKVAppExtension",
            dependencies: ["MMKVCore"],
            path: "iOS/MMKV/MMKV/MMKVAppExtension",
            resources: [
                // NEW: Required for App Store submission in 2025
                .copy("../Resources/PrivacyInfo.xcprivacy") 
            ],
            publicHeadersPath: "include",
            cSettings: [
                .define("MMKV_IOS_EXTENSION")
            ],
            cxxSettings: [
                .headerSearchPath("."),
                .unsafeFlags(["-x", "objective-c++", "-fno-objc-arc", "-fapplication-extension"])
            ],
            linkerSettings: [
                .linkedLibrary("z"),
                .linkedLibrary("c++")
            ]
        ),
    ],
    cxxLanguageStandard: .cxx20
)
