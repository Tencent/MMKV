// swift-tools-version: 5.9
import PackageDescription

// Local demos can use checkout sources without changing the published dependency.
let mmkvDependency: Package.Dependency
if let localPath = Context.environment["MMKV_LOCAL_PACKAGE_PATH"], !localPath.isEmpty {
    mmkvDependency = .package(name: "MMKV", path: localPath)
} else {
    mmkvDependency = .package(url: "https://github.com/Tencent/MMKV.git", from: "2.4.2")
    // mmkvDependency = .package(url: "https://github.com/Tencent/MMKV.git", branch: "dev")
}

let package = Package(
    name: "mmkv_ios",
    platforms: [
        .iOS("13.0"),
        .macOS("10.15")
    ],
    products: [
        // Dart resolves the bridge functions through DynamicLibrary.process().
        // A dynamic product keeps those symbols visible to dlsym on Apple platforms.
        .library(name: "mmkv-ios", type: .dynamic, targets: ["mmkv_ios"])
    ],
    dependencies: [
        mmkvDependency,
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
                .linkedFramework("UIKit", .when(platforms: [.iOS])),
                .linkedFramework("AppKit", .when(platforms: [.macOS])),
                .linkedFramework("Flutter", .when(platforms: [.iOS])),
                .linkedFramework("FlutterMacOS", .when(platforms: [.macOS])),
            ]
        )
    ]
)
