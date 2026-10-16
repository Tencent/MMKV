# MMKV Kotlin Multiplatform

MMKV provides experimental Kotlin Multiplatform support for Android and iOS.
The API and published artifact layout may change in a future release.

## Gradle dependency

Add the MMKV KMP package to your shared module:

```kotlin
kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation("com.tencent:mmkv-kmp:2.4.3")
        }
    }
}
```

Android consumers should have AndroidX enabled in `gradle.properties`:

```properties
android.useAndroidX=true
```

Supported targets in v2.4.3:

* Android
* `iosArm64`
* `iosSimulatorArm64`
* `iosX64`

The iOS deployment target is 13.0. Apple Silicon simulator runtimes start at
iOS 14.

## Published artifacts

For this experimental release, consumers should depend only on the root artifact:

```text
com.tencent:mmkv-kmp:2.4.3
```

Gradle uses Kotlin Multiplatform metadata to select the target artifact:

```text
com.tencent:mmkv-kmp-android:2.4.3
com.tencent:mmkv-kmp-iosarm64:2.4.3
com.tencent:mmkv-kmp-iossimulatorarm64:2.4.3
com.tencent:mmkv-kmp-iosx64:2.4.3
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

`mmkv-kmp` also bundles an AndroidX App Startup `Initializer` that captures the
application `Context` automatically in the default process, with no manifest
edits, code, or Gradle configuration beyond the dependency. This lets you call
the Context-free overload instead:

```kotlin
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize

MMKV.initialize() // uses the automatically captured Context
```

If this overload is invoked before any `Context` was captured (for example, if
the App Startup provider was disabled, or in a secondary process that never
declared it), it throws `IllegalStateException` naming `initialize(context, ...)`
as the fallback — it never throws `NullPointerException` and never silently
proceeds with a null or default `Context`.

For a secondary process, declare an extra `<provider>` entry with the same
authority in your app's manifest so App Startup also runs there:

```xml
<provider
    android:name="androidx.startup.InitializationProvider"
    android:authorities="${applicationId}.androidx-startup"
    android:exported="false"
    android:process=":your_secondary_process"
    tools:node="merge">
    <meta-data
        android:name="com.tencent.mmkv.kmp.MMKVContextInitializer"
        android:value="androidx.startup" />
</provider>
```

To opt out of automatic capture entirely and always call `MMKV.initialize(context, ...)`
explicitly, remove the `<meta-data>` entry from your manifest:

```xml
<provider
    android:name="androidx.startup.InitializationProvider"
    android:authorities="${applicationId}.androidx-startup"
    tools:node="merge">
    <meta-data
        android:name="com.tencent.mmkv.kmp.MMKVContextInitializer"
        tools:node="remove" />
</provider>
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

Android delegates to the native `com.tencent:mmkv:2.4.3` AAR. iOS embeds MMKV
Core through the C bridge in the published native KLIBs, so consumers do not
need CocoaPods, Swift Package Manager, or a source build.

Do not also link the native MMKV CocoaPod or SwiftPM product into the same iOS
binary that consumes `mmkv-kmp`; both contain MMKV Core and can produce
duplicate native symbols.

## Build from source

Building the KMP project and its iOS targets requires macOS, Xcode command-line
tools, CMake, and JDK 21. The checked-in Gradle daemon JVM criteria select JDK 21
for both command-line builds and Android Studio imports; install it before syncing.

If Android Studio cannot find `cmake` on its PATH, set `cmake.dir` to the CMake
installation directory containing `bin/cmake` in the untracked
`KMP/local.properties`. For example, with Homebrew on Apple Silicon:

```properties
cmake.dir=/opt/homebrew
```

Both native configure and build tasks use this path. When `cmake.dir` is unset,
they use `cmake` from PATH.

Open the `KMP` directory in Android Studio. The sample is enabled by
`MMKV_INCLUDE_SAMPLE=true` in `gradle.properties`. After syncing, select the
Android App run configuration for `:sample:composeApp` to run the Android sample.
Use `-PMMKV_INCLUDE_SAMPLE=false` for a library-only build.

The sample enables Android and iOS by default. Its desktop configuration is
disabled because the MMKV wrapper does not yet provide a JVM target. The existing
desktop scaffold is retained behind `-PMMKV_ENABLE_DESKTOP=true` for development;
it still requires JVM support in the wrapper before it can build. Published-MMKV
mode always disables desktop, even when this flag is set.

```bash
cd KMP
./gradlew :mmkv:assemble -PMMKV_USE_MAVEN_LOCAL=true
```

`MMKV_USE_MAVEN_LOCAL=true` is needed when the matching
`com.tencent:mmkv:2.4.3` Android artifact has been published only to Maven
Local.

To prepare the iOS sample, install CocoaPods and run the following from `KMP`:

```bash
./gradlew :sample:composeApp:podInstall -PMMKV_INCLUDE_SAMPLE=true
open sample/iosApp/iosApp.xcworkspace
```

This generates the Pods support files, including the `.xcconfig` files referenced
by the Xcode project. Use the generated workspace for Xcode builds. The sample is
included automatically when Xcode invokes Gradle through the CocoaPods build script.

When running the iOS sample from Android Studio, its linked Xcode project must
use `sample/iosApp/iosApp.xcworkspace` as well. If the IDE linked the `.xcodeproj`
before `podInstall` created the workspace, invoke **Unlink Xcode project** via
**Find Action**, then **Sync Project with Gradle Files** to detect the workspace.
Building the standalone project omits the Pods target and can fail with
`Framework 'Pods_iosApp' not found`.

Release maintainers should also follow [PUBLISHING.md](./PUBLISHING.md).
