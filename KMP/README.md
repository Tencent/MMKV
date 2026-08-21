# MMKV Kotlin Multiplatform

MMKV provides experimental Kotlin Multiplatform support for Android and iOS.
The API and published artifact layout may change in a future release.

## Gradle dependency

Add the MMKV KMP package to your shared module:

```kotlin
kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation("com.tencent:mmkv-kmp:2.4.2")
        }
    }
}
```

Android consumers should have AndroidX enabled in `gradle.properties`:

```properties
android.useAndroidX=true
```

Supported targets in v2.4.2:

* Android
* `iosArm64`
* `iosSimulatorArm64`
* `iosX64`

The iOS deployment target is 13.0. Apple Silicon simulator runtimes start at
iOS 14.

## Published artifacts

For this experimental release, consumers should depend only on the root artifact:

```text
com.tencent:mmkv-kmp:2.4.2
```

Gradle uses Kotlin Multiplatform metadata to select the target artifact:

```text
com.tencent:mmkv-kmp-android:2.4.2
com.tencent:mmkv-kmp-iosarm64:2.4.2
com.tencent:mmkv-kmp-iossimulatorarm64:2.4.2
com.tencent:mmkv-kmp-iosx64:2.4.2
```

Do not add the target-specific artifacts directly.

## Initialize

Android:

```kotlin
import android.app.Application
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize

class App : Application() {
    override fun onCreate() {
        super.onCreate()
        MMKV.initialize(this)
    }
}
```

iOS:

```kotlin
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize

MMKV.initialize()
```

## Basic usage

```kotlin
val kv = MMKV.defaultMMKV()
kv.encodeString("name", "MMKV")
val name = kv.decodeString("name")
```

`close()` permanently destroys the native MMKV instance. All references backed
by the same native instance become invalid immediately. The caller must ensure
that no operation is running and no reference is used afterward. Discard every
reference before reopening the same ID.

Android delegates to the native `com.tencent:mmkv:2.4.2` AAR. iOS embeds MMKV
Core through the C bridge in the published native KLIBs, so consumers do not
need CocoaPods, Swift Package Manager, or a source build.

Do not also link the native MMKV CocoaPod or SwiftPM product into the same iOS
binary that consumes `mmkv-kmp`; both contain MMKV Core and can produce
duplicate native symbols.

## Build from source

Building the KMP project and its iOS targets requires macOS, Xcode command-line
tools, CMake, and JDK 11 or newer:

```bash
cd KMP
./gradlew :mmkv:assemble -PMMKV_USE_MAVEN_LOCAL=true
```

`MMKV_USE_MAVEN_LOCAL=true` is needed when the matching
`com.tencent:mmkv:2.4.2` Android artifact has been published only to Maven
Local.

Release maintainers should also follow [PUBLISHING.md](./PUBLISHING.md).
