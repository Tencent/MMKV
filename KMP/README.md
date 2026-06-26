# MMKV Kotlin Multiplatform

MMKV provides a Kotlin Multiplatform wrapper for Android and iOS.

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

Supported targets in v2.4.1:

* Android
* `iosArm64`
* `iosSimulatorArm64`
* `iosX64`

## Initialize

Android:

```kotlin
class App : Application() {
    override fun onCreate() {
        super.onCreate()
        MMKV.initialize(this)
    }
}
```

iOS:

```kotlin
MMKV.initialize()
```

## Basic usage

```kotlin
val kv = MMKV.defaultMMKV()
kv.encodeString("name", "MMKV")
val name = kv.decodeString("name")
```

Android delegates to the native `com.tencent:mmkv:2.4.1` AAR. iOS embeds MMKV Core through the C bridge in the published native KLIBs, so consumers can use the Gradle package directly as a ready-to-use Gradle dependency.
