// swift-tools-version: 5.9
import PackageDescription

// Local demos can use checkout sources without changing the published dependency.
let mmkvDependency: Package.Dependency
if let localPath = Context.environment["MMKV_LOCAL_PACKAGE_PATH"], !localPath.isEmpty {
    mmkvDependency = .package(name: "MMKV", path: localPath)
} else {
    mmkvDependency = .package(url: "https://github.com/Tencent/MMKV.git", from: "2.4.1")
    // mmkvDependency = .package(url: "https://github.com/Tencent/MMKV.git", branch: "dev")
}

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
            ]
        )
    ]
)
