# MMKV Kotlin Multiplatform

MMKV now ships a Kotlin Multiplatform wrapper under `KMP/`, exposing one common Kotlin API in `com.tencent.mmkv.kmp` while delegating to the native MMKV implementation on each target.

## Supported targets

| Target | Backend |
| --- | --- |
| `androidTarget()` | Published `com.tencent:mmkv` Android library via JNI |
| `iosArm64`, `iosSimulatorArm64`, `iosX64` | CocoaPods `MMKV` framework |
| `macosArm64`, `macosX64` | CocoaPods `MMKV` framework |
| `linuxX64`, `linuxArm64`, `mingwX64` | Kotlin/Native cinterop against the MMKV C bridge |
| `jvm("desktop")` | JNA against a packaged `libmmkv-kmp` shared library |

`watchOS`, `tvOS`, `visionOS`, and HarmonyOS are not part of the KMP release scope.

## Maven coordinates

Core KMP artifact:

```kotlin
implementation("com.tencent:mmkv-kmp:2.4.0")
```

JVM desktop runtime needs a host-specific native artifact on the classpath:

```kotlin
runtimeOnly("com.tencent:mmkv-kmp-desktop-native-macos-arm64:2.4.0")
runtimeOnly("com.tencent:mmkv-kmp-desktop-native-macos-x86_64:2.4.0")
runtimeOnly("com.tencent:mmkv-kmp-desktop-native-linux-x86_64:2.4.0")
runtimeOnly("com.tencent:mmkv-kmp-desktop-native-windows-x86_64:2.4.0")
```

Only add the runtime artifact for your desktop host. Android, Apple, and Kotlin/Native desktop targets do not need it.

## Local build

```bash
cd KMP
./gradlew :mmkv:build
./gradlew :mmkv:allTests
```

The build invokes CMake automatically for native desktop targets. No manual `cmake` pre-step or `-Djna.library.path=...` workaround is required anymore.

## Quick start

```kotlin
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.MMKVConfig

val kv = MMKV.mmkvWithID("demo")
kv.encodeString("hello", "world")
println(kv.decodeString("hello"))

val encrypted = MMKV.mmkvWithID("secure", MMKVConfig(cryptKey = "my-secret-16b"))
```

Platform-specific `MMKV.initialize(...)` extensions live in each platform source set. The sample app under `KMP/sample/composeApp/` shows Android, iOS, and desktop bootstrap code.

## Platform notes

### Android

The KMP Android target delegates to `com.tencent:mmkv:$MMKV_VERSION`. Consumer ProGuard rules are provided by `consumer-rules.pro`.

### iOS / macOS

The KMP Darwin targets consume the CocoaPods `MMKV` pod. If your app also links MMKV directly through SPM or CocoaPods outside the KMP wrapper, you can end up with duplicate Objective-C classes at runtime.

### JVM desktop

The desktop target extracts `libmmkv-kmp` from classpath resources automatically before JNA loads it. The host-specific runtime artifact is what provides that resource on published builds.

### Native desktop

Linux and Windows Kotlin/Native targets bundle `libmmkv-kmp.a` into the generated cinterop klib. Gradle builds that archive through the `cmakeBuild<target>` tasks before cinterop runs.

## Release notes

Publishing is wired in Gradle, but the repository does not currently carry the GitHub Actions workflows for CI or release publication.

Kotlin Multiplatform publication is still host-dependent:
- Linux should publish metadata, Android, JVM desktop, and Linux native artifacts.
- macOS should publish iOS and macOS artifacts.
- Windows should publish the Mingw artifact.
- Each desktop host should publish its host-specific `mmkv-kmp-desktop-native-<os>-<arch>` runtime JAR.

`linuxArm64` is wired in the project and compiles locally, but publishing it still needs either an ARM64 Linux publisher or an explicit cross-toolchain setup.

## Known limitations

| Area | Limitation | Reason |
| --- | --- | --- |
| Darwin | `lock()` / `unlock()` / `tryLock()` are no-ops, and `tryLock()` returns `false`. | The ObjC MMKV API does not expose these methods. |
| Darwin | `isExpirationEnabled` and `isCompareBeforeSetEnabled` always return `false`. | The ObjC MMKV API does not expose those status flags. |
| Android | `isExpirationEnabled`, `isEncryptionEnabled`, and `isCompareBeforeSetEnabled` still rely on reflection. | The upstream Android MMKV surface keeps them private-native. |
| JVM desktop | `registerHandler()` / `unRegisterHandler()` are still no-ops. | JNA callback bridging for the unified handler is not implemented yet. |
| JVM desktop | `setLogLevel()` is a no-op after initialization. | The C bridge configures log level during `initialize()`. |
