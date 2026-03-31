// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "mmkv_ios",
    platforms: [
        .iOS("13.0"),
        .macOS("10.15")
    ],
    products: [
        .library(name: "mmkv-ios", targets: ["mmkv_ios"])
    ],
    dependencies: [
        .package(url: "https://github.com/Tencent/MMKV.git", from: "2.4.0"),
    ],
    targets: [
        .target(
            name: "mmkv_ios",
            dependencies: [
                .product(name: "MMKV", package: "MMKV"),
            ],
            cSettings: [
                .headerSearchPath("include/mmkv_ios"),
            ],
            cxxSettings: [
                .headerSearchPath("include/mmkv_ios"),
            ],
            linkerSettings: [
                .linkedLibrary("z"),
                .linkedLibrary("c++"),
            ]
        )
    ]
)
