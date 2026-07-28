# MMKV Kotlin Multiplatform

MMKV provides a ready-to-use Kotlin Multiplatform package for Android and iOS.

## Gradle dependency

Add the MMKV KMP package to your shared module:

```kotlin
kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation("com.tencent:mmkv-kmp:2.4.1")
        }
    }
}
```

Android consumers should have AndroidX enabled in `gradle.properties`:

```properties
android.useAndroidX=true
```

Supported targets in v2.4.1:

* Android
* `iosArm64`
* `iosSimulatorArm64`
* `iosX64`

The iOS deployment target is 13.0. Apple Silicon simulator runtimes start at
iOS 14.

## Published artifacts

Consumers should depend only on the root artifact:

```text
com.tencent:mmkv-kmp:2.4.1
```

Gradle uses Kotlin Multiplatform metadata to select the target artifact:

```text
com.tencent:mmkv-kmp-android:2.4.1
com.tencent:mmkv-kmp-iosarm64:2.4.1
com.tencent:mmkv-kmp-iossimulatorarm64:2.4.1
com.tencent:mmkv-kmp-iosx64:2.4.1
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

`MMKV` implements `AutoCloseable`, so scoped ownership can use Kotlin's
`use {}` helper. `close()` is terminal and idempotent on that wrapper, but
it invalidates all wrappers backed by the same native instance. Do not race
it with another operation, and discard all aliases before reopening the ID.

```kotlin
MMKV.mmkvWithID("scoped").use { kv ->
    kv.encodeString("name", "MMKV")
}
```

Android delegates to the native `com.tencent:mmkv:2.4.1` AAR. iOS embeds MMKV
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
`com.tencent:mmkv:2.4.1` Android artifact has been published only to Maven
Local.

Release maintainers should also follow [PUBLISHING.md](./PUBLISHING.md).
