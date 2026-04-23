# MMKV — Kotlin Multiplatform

A Kotlin Multiplatform wrapper around MMKV, providing a single idiomatic Kotlin API that delegates to the native MMKV implementation on each target. The public API lives in `commonMain` (`com.tencent.mmkv.kmp.MMKV`).

> ⚠️ **Preview.** Not yet published to Maven Central. Consume as an included build or from a local `./gradlew publishToMavenLocal` for now.

## Supported targets

| Target | Backend |
|---|---|
| `androidTarget` | Delegates to the published [`com.tencent:mmkv`](https://central.sonatype.com/artifact/com.tencent/mmkv) Android library via JNI. |
| `iosArm64`, `iosSimulatorArm64`, `iosX64` | CocoaPods `MMKV` framework via Kotlin/Native ObjC interop. |
| `macosArm64`, `macosX64` | CocoaPods `MMKV` framework via Kotlin/Native ObjC interop. |
| `linuxX64`, `linuxArm64` | Kotlin/Native cinterop against the C bridge (`libmmkv-kmp.a`). |
| `mingwX64` | Kotlin/Native cinterop against the C bridge. |
| `jvm("desktop")` | JNA loading the host's `libmmkv-kmp` shared library. |

`watchOS`, `tvOS`, and `visionOS` are not currently in scope — add them if you need them.

## Installation

Publishing is not yet wired up. Until it is, the options are:

**Gradle included build (recommended):**
```kotlin
// settings.gradle.kts in your consumer project
includeBuild("path/to/MMKV/KMP") {
    dependencySubstitution {
        substitute(module("com.tencent:mmkv-kmp")).using(project(":mmkv"))
    }
}
```

**Local Maven:**
```bash
cd KMP && ./gradlew :mmkv:publishToMavenLocal
```
then in your consumer:
```kotlin
repositories { mavenLocal() }
dependencies { implementation("com.tencent:mmkv-kmp:2.4.0") }
```

## Build from source

```bash
cd KMP
./gradlew :mmkv:build
```

On a fresh checkout the Gradle build will automatically invoke CMake for native desktop targets via the `cmakeBuild<Target>` tasks — no manual `cmake` pre-step is required. The CMake scripts live under `mmkv/nativeInterop/`.

Requirements: CMake 3.14+, a C++20 toolchain, JDK 11+. For Apple targets, CocoaPods is required (`brew install cocoapods`).

### Cross-building `linuxArm64`

Kotlin/Native can compile Kotlin sources for `linuxArm64` from any host, but the native-desktop static library (`libmmkv-kmp.a`) needs an ARM64 toolchain. On a non-ARM Linux host, pass `-DCMAKE_TOOLCHAIN_FILE=...` to the `cmakeBuildLinuxArm64` task or build it on an ARM64 machine.

## Quick start

```kotlin
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.MMKVConfig

// Platform-specific initialize(). See the sample app for Android/iOS/desktop wiring.
val mmkv = MMKV.mmkvWithID("my-store")
mmkv.encodeString("greeting", "hello")
val greeting: String? = mmkv.decodeString("greeting")
```

Encryption:
```kotlin
val secured = MMKV.mmkvWithID("secrets", MMKVConfig(cryptKey = "my-secret-16b"))
```

A full Compose Multiplatform sample demonstrating Android, iOS, and desktop integration lives under `KMP/sample/composeApp/`.

## Platform notes

### Android
* Delegates to `com.tencent:mmkv:$MMKV_VERSION` — any fix landed in the Android MMKV library propagates automatically when that library's version bumps.
* Consumer ProGuard/R8 rules (`consumer-rules.pro`) are merged into consumer apps automatically. They keep MMKV's JNI-reachable methods and the KMP wrapper surface.

### iOS / macOS
* Delegates to the CocoaPods `MMKV` pod. **If your app also links MMKV directly via SPM or CocoaPods, you'll get duplicate ObjC class warnings and the runtime may crash on startup.** Depend on MMKV through the KMP wrapper **only**.
* Deployment targets: iOS 13.0, macOS 10.15.

### JVM desktop
* The Gradle build bundles a host-only `libmmkv-kmp.{dylib,so,dll}` into the output JAR under `resources/native/<os>-<arch>/`. **Runtime auto-extraction to `java.library.path` is not wired up yet** — point JNA at the build directory manually until it is:
  ```bash
  java -Djna.library.path=KMP/mmkv/nativeInterop/build/desktop<OS>Shared ...
  ```
* A cross-OS/arch classifier-JAR layout (mac-arm64 / mac-x64 / linux-x64 / linux-arm64 / windows-x64) will come with publishing.

### Native desktop (Linux / Windows)
* Kotlin/Native links against `libmmkv-kmp.a` produced by the Gradle CMake task. Build output goes to `mmkv/nativeInterop/build/<target>/`.

## Version alignment

All MMKV version strings (the CocoaPods dependency, the Android dependency, and the CMake `FetchContent` tag) read from `KMP/gradle.properties`:
```
MMKV_VERSION=2.4.0
```
Bump it in one place to track a new MMKV release. The generated `mmkv.podspec` is regenerated on Gradle sync and picks up the same value.

## Known limitations

| Area | Limitation | Why |
|---|---|---|
| Darwin | `lock()` / `unlock()` / `tryLock()` are no-ops, `tryLock()` returns `false`. | The upstream ObjC MMKV framework does not expose these methods. Use a platform lock directly if you need inter-process synchronization on iOS/macOS. |
| Darwin | `isExpirationEnabled` and `isCompareBeforeSetEnabled` always return `false`. | Same — not exposed in the ObjC API. |
| Android | `isExpirationEnabled`, `isEncryptionEnabled`, `isCompareBeforeSetEnabled` use reflection on private-native methods. | The upstream Android MMKV class declares these as `private native`. Will switch to a public API when upstream exposes one. |
| JVM desktop | `registerHandler()` / `unRegisterHandler()` are no-ops. | JNA callback bridging for the unified `MMKVHandler` isn't implemented yet. |
| JVM desktop | `setLogLevel()` is a no-op after initialization. | Log level is configured during `initialize()` on the C bridge. |
| JVM desktop | `decodeBool()` and `containsKey()` can return indeterminate results. | The JNA interface declares these as Kotlin `Boolean`, which JNA marshals as 4-byte `int`. The C bridge returns 1-byte `bool`, so the upper 3 bytes of the return register are ABI-indeterminate. The commonTest suite gates boolean assertions behind `MMKVTestEnv.hasKnownBoolRoundTripIssue` until this is fixed by a proper `TypeMapper` / `BYTE`-sized wrapper in `MMKV.desktop.kt`. |

## Tests

```bash
cd KMP
./gradlew :mmkv:allTests
```

The smoke suite lives in `mmkv/src/commonTest/` and covers primitives round-trip, decode defaults, key management, `allKeys`, and encrypted-instance round-trip. It runs on every reachable target. Android unit tests skip because MMKV native init requires a `Context` that a host JVM unit test doesn't have — add instrumented tests if you need Android coverage.

## Contributing

The KMP module is under active polish; publishing, CI, and the Android / ObjC public-API expansions tracked in `report_kmp/` are explicit follow-ups.
