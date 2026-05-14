# MMKV for Kotlin Multiplatform

MMKV is an **efficient**, **small**, **easy-to-use** key-value storage framework used in the WeChat application. The Kotlin Multiplatform wrapper exposes one common Kotlin API in `com.tencent.mmkv.kmp` for Android, iOS, macOS, Linux, Windows, and JVM desktop.

## Features

* **Efficient**. MMKV uses mmap to keep memory synced with files and protobuf to encode/decode values.
  * **Multi-process concurrency**: MMKV supports concurrent read-read and read-write access between processes.

* **Easy to use**. You can use MMKV as you go. Changes are saved immediately; no `sync` or `apply` calls are required.

* **Multiplatform**. One Kotlin API covers Android, iOS, macOS, Linux, Windows, and JVM desktop.

## Getting Started

### Installation

Add the common KMP artifact to your shared module:

```kotlin
kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation("com.tencent:mmkv-kmp:2.4.0")
        }
    }
}
```

For JVM desktop, also add the native runtime artifacts for the platforms you ship to the desktop source set:

```kotlin
kotlin {
    sourceSets {
        val desktopMain by getting {
            dependencies {
                runtimeOnly("com.tencent:mmkv-kmp-desktop-native-macos-arm64:2.4.0")
                runtimeOnly("com.tencent:mmkv-kmp-desktop-native-macos-x86_64:2.4.0")
                runtimeOnly("com.tencent:mmkv-kmp-desktop-native-linux-x86_64:2.4.0")
                runtimeOnly("com.tencent:mmkv-kmp-desktop-native-windows-x86_64:2.4.0")
            }
        }
    }
}
```

For a single-platform desktop app, include only the matching runtime artifact. For a multi-platform desktop distribution, include every runtime artifact you plan to ship; MMKV loads the matching native library at runtime.

Android, iOS, macOS, Linux, and Windows Kotlin/Native targets do not need the JVM desktop runtime artifacts.

### Supported targets

| Target | Status |
| --- | --- |
| `android` | Supported |
| `iosArm64`, `iosSimulatorArm64`, `iosX64` | Supported |
| `macosArm64`, `macosX64` | Supported |
| `linuxX64`, `linuxArm64`, `mingwX64` | Supported |
| `jvm("desktop")` | Supported with host-specific runtime artifact |

`watchOS`, `tvOS`, `visionOS`, and HarmonyOS NEXT are not currently supported.

### Setup

Initialize MMKV once during app startup before accessing any MMKV instance.

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

iOS / macOS:

```kotlin
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize

fun initializeStorage() {
    MMKV.initialize()
}
```

JVM desktop and Kotlin/Native desktop:

```kotlin
import com.tencent.mmkv.kmp.MMKV
import com.tencent.mmkv.kmp.initialize

fun initializeStorage(rootDir: String) {
    MMKV.initialize(rootDir)
}
```

### CRUD Operations

MMKV has a global default instance that can be used directly:

```kotlin
import com.tencent.mmkv.kmp.MMKV

val kv = MMKV.defaultMMKV()

kv.encodeBool("bool", true)
println("bool = ${kv.decodeBool("bool")}")

kv.encodeInt("int", Int.MAX_VALUE)
println("int = ${kv.decodeInt("int")}")

kv.encodeLong("long", Long.MAX_VALUE)
println("long = ${kv.decodeLong("long")}")

kv.encodeString("string", "Hello Kotlin Multiplatform from MMKV")
println("string = ${kv.decodeString("string")}")

val bytes = "Hello bytes".encodeToByteArray()
kv.encodeBytes("bytes", bytes)
println("bytes = ${kv.decodeBytes("bytes")?.decodeToString()}")
```

Deleting and querying:

```kotlin
kv.removeValueForKey("bool")
println("contains bool: ${kv.containsKey("bool")}")

kv.removeValuesForKeys(listOf("int", "long"))
println("all keys: ${kv.allKeys}")
```

If different modules need isolated storage, create an instance with a unique ID:

```kotlin
val userKv = MMKV.mmkvWithID("user_profile")
userKv.encodeString("name", "MMKV")
```

For multi-process access, create the instance with multi-process mode:

```kotlin
import com.tencent.mmkv.kmp.MMKVConfig
import com.tencent.mmkv.kmp.MMKVMode

val multiProcessKv = MMKV.mmkvWithID(
    "shared_storage",
    MMKVConfig(mode = MMKVMode.MULTI_PROCESS)
)
```

### Supported Types

* Primitive types:
  - `Boolean`, `Int`, `Long`, `Float`, `Double`

* Classes and arrays:
  - `String`, `ByteArray`

* Key list APIs:
  - `List<String>` for `allKeys`, `allNonExpiredKeys()`, and `removeValuesForKeys(...)`

### Encryption

By default, MMKV stores key-values in plain text inside the app sandbox. Use `cryptKey` to create an encrypted instance:

```kotlin
val encrypted = MMKV.mmkvWithID(
    "secure_storage",
    MMKVConfig(cryptKey = "MyEncryptKey")
)
```

You can change the encryption key later, or switch an instance between encrypted and unencrypted states:

```kotlin
val kv = MMKV.mmkvWithID("secure_storage")

// change from unencrypted to encrypted with AES-128 key length
kv.reKey("Key_seq_1")

// change encryption key with AES-256 key length
kv.reKey("Key_Seq_Very_Looooooooong", aes256 = true)

// change from encrypted to unencrypted
kv.reKey(null)
```

### Customize Location

You can customize the root directory during initialization. Android requires a `Context`; other targets use the platform-specific `initialize` overload without one:

```kotlin
// Android
MMKV.initialize(context, rootDir = "/path/to/mmkv")

// iOS, macOS, JVM desktop, Kotlin/Native desktop
MMKV.initialize(rootDir = "/path/to/mmkv")
```

You can also customize an individual instance location:

```kotlin
val kv = MMKV.mmkvWithID(
    "custom_storage",
    MMKVConfig(rootPath = "/path/to/custom/mmkv")
)
```

It is recommended to store MMKV files inside your app's sandbox path.

### Namespace

Use `MMKVNameSpace` to group instances under a custom root directory:

```kotlin
import com.tencent.mmkv.kmp.MMKVNameSpace

val namespace = MMKVNameSpace.of("/path/to/mmkv_namespace")
val kv = namespace.mmkvWithID("profile")
kv.encodeString("name", "MMKV")
```

### Log

By default, MMKV prints logs to the platform console. You can configure the log level during initialization:

```kotlin
import com.tencent.mmkv.kmp.MMKVLogLevel

MMKV.initialize(rootDir = "/path/to/mmkv", logLevel = MMKVLogLevel.None)
```

You can also register a handler for log redirection and MMKV callbacks on supported targets:

```kotlin
import com.tencent.mmkv.kmp.MMKVHandler
import com.tencent.mmkv.kmp.MMKVLogLevel

MMKV.registerHandler(object : MMKVHandler() {
    override fun wantLogRedirect(): Boolean = true

    override fun mmkvLog(
        level: MMKVLogLevel,
        file: String,
        line: Int,
        function: String,
        message: String,
    ) {
        println("[$level] $message")
    }
})
```

## Additional docs

The sample app under `KMP/sample/composeApp/` shows Android, iOS, and desktop bootstrap code.

## License

MMKV is published under the BSD 3-Clause license. For details, check out the [LICENSE.TXT](../LICENSE.TXT).

## Change Log

Check out the [CHANGELOG.md](../CHANGELOG.md) for details of change history.

## Contributing

If you are interested in contributing, check out the [CONTRIBUTING.md](../CONTRIBUTING.md) and [Code of Conduct](../CODE_OF_CONDUCT.md).

## FAQ & Feedback

Check out the [FAQ](https://github.com/Tencent/MMKV/wiki/FAQ) first. If you have questions or issues, please create an issue on GitHub.
