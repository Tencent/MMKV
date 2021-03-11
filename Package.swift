// swift-tools-version:5.1
import PackageDescription

let package = Package(
    name: "MMKV",
    platforms: [
        .iOS(.v9),
        .macOS("10.9")
    ],
    products: [
        .library(
            name: "MMKV",
            targets: ["MMKV"]),
        .library(
            name: "MMKVCore",
            targets: ["MMKVCore"])
    ],
    dependencies: [
        
    ],
    targets: [
        .target(
            name: "MMKV",
            dependencies: []),
        .testTarget(
            name: "MyLibraryTests",
            dependencies: ["MyLibrary"]),
    ]
)
