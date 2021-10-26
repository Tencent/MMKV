# MMKV Change Log

## v1.2.11 / 2021-10-26

### Android
* Due to increasing report about crash inside STL, we have decided to make MMKV **static linking** `libc++` **by default**. Starting from v1.2.11, `com.tencent:mmkv-static` is the same as `com.tencent:mmkv`.
* For those still in need of MMKV with shared linking of `libc++_shared`, you could use `com.tencent:mmkv-shared` instead.
* Add backup & restore ability.

### iOS / macOS
* Add backup & restore ability.
* Support tvOS.
* Fix a compile error on some old Xcode.

### Flutter (v1.2.12)
* Add backup & restore ability.

### POSIX / golang / Python
* Add backup & restore ability.
* Fix a compile error on Gentoo.

### Win32
* Add backup & restore ability.

## v1.2.10 / 2021-06-25
This version is mainly for Android & Flutter.  

### Android
* Complete **JavaDoc documentation** for all public methods, classes, and interfaces. From now on, you can find the [API reference online](https://javadoc.io/doc/com.tencent/mmkv).
* Drop the support of **armeabi** arch. Due to some local build cache mistake, the last version (v1.2.9) of MMKV still has an unstripped armeabi arch inside. This is fixed.
* Change `MMKV.mmkvWithID()` from returning `null` to throwing exceptions on any error.
* Add `MMKV.actualSize()` to get the actual used size of the file.
* Mark `MMKV.commit()` & `MMKV.apply()` as deprecated, to avoid some misuse after migration from SharedPreferences to MMKV.

### Flutter (v1.2.11)
* Bug Fixed: When building on iOS, occasionally it will fail on symbol conflict with other libs. We have renamed all public native methods to avoid potential conflict.
* Keep up with MMKV native lib v1.2.10.

## v1.2.9 / 2021-05-26
This version is mainly for Android & Flutter.  

### Android
* Drop the support of **armeabi** arch. As has been mention in the last release, to avoid some crashes on the old NDK (r16b), and make the most of a more stable `libc++`, we have decided to upgrade MMKV's building NDK in this release. That means we can't support **armeabi** anymore. Those who still in need of armeabi can **build from sources** by following the [instruction in the wiki](https://github.com/Tencent/MMKV/wiki/android_setup).

We really appreciate your understanding.

### Flutter (v1.2.10)
* Bug Fixed: When calling `MMKV.encodeString()` with an empty string value on Android, `MMKV.decodeString()` will return `null`.
* Bug Fixed: After **upgrading** from Flutter 1.20+ to 2.0+, calling `MMKV.defaultMMKV()` on Android might fail to load, you can try calling `MMKV.defaultMMKV(cryptKey: '\u{2}U')` with an **encrytion key** '\u{2}U' instead.
* Keep up with MMKV native lib v1.2.9.

## v1.2.8 / 2021-05-06
This will be the last version that supports **armeabi arch** on Android. To avoid some crashes on the old NDK (r16b), and make the most of a more stable `libc++`, we have decided to upgrade MMKV's building NDK in the next release. That means we can't support **armeabi** anymore.  

We really appreciate your understanding.

### Android
* Migrate MMKV to Maven Central Repository. For versions older than v1.2.7 (including), they are still available on JCenter.
* Add `MMKV.disableProcessModeChecker()`. There are some native crash reports due to the process mode checker. You can disable it manually.
* For the same reason described above (native crashed), MMKV will now turn off the process mode checker on a non-debuggable app (aka, a release build).
* For MMKV to detect whether the app is debuggable or not, when calling `MMKV.initialize()` to customize the root directory, a `context` parameter is required now.

### iOS / macOS
* Min iOS support has been **upgrade to iOS 9**.
* Support building by Xcode 12.

### Flutter (v1.2.9)
* Support null-safety.
* Upgrade to flutter 2.0.
* Fix a crash on the iOS when calling `encodeString()` with an empty string value.

**Known Issue on Flutter**  

* When calling `encodeString()` with an empty string value on Android, `decodeString()` will return `null`. This bug will be fixed in the next version of Android Native Lib. iOS does not have such a bug.

### Win32
* Fix a compile error on Visual Studio 2019.

## v1.2.7 / 2020-12-25
Happy holidays everyone!
 
### Changes for All platforms
* Fix a bug when calling `sync()` with `false ` won't do `msync()` asynchronous and won't return immediately.

### Android
* Fix an null pointer exception when calling `putStringSet()` with `null`.
* Complete review of all MMKV methods about Java nullable/nonnull annotation.
* Add API for `MMKV.initialize()` with both `Context` and `LibLoader` parammeters.

### Flutter (v1.2.8)
* Fix a crash on the iOS simulator when accessing the default MMKV instance.
* Fix a bug on iOS when initing the default MMKV instance with a crypt key, the instance is still in plaintext.

### Golang
Add golang for POSIX platforms. Most things actually work!. Check out the [wiki](https://github.com/Tencent/MMKV/wiki/golang_setup) for information.

## v1.2.6 / 2020-11-27
### Changes for All platforms
* Fix a file corruption when calling `reKey()` after `removeKeys()` has just been called.

### Android
* Fix compile error when `MMKV_DISABLE_CRYPT` is set.
* Add a preprocess directive `MMKV_DISABLE_FLUTTER` to disable flutter plugin features. If you integrate MMKV by source code, and if you are pretty sure the flutter plugin is not needed, you can turn that off to save some binary size.

### Flutter (v1.2.7)
Add MMKV support for **Flutter** on iOS & Android platform.  Most things actually work!  
Check out the [wiki](https://github.com/Tencent/MMKV/wiki/flutter_setup) for more info.

## v1.2.5 / 2020-11-13
This is a pre-version for Flutter. The official Flutter plugin of MMKV will come out soon. Stay Tune!

### iOS / macOS
* Fix an assert error of encrypted MMKV when encoding some `<NSCoding>` objects.
* Fix a potential leak when decoding duplicated keys from the file.
* Add `+[MMKV pageSize]`, `+[MMKV version]` methods.
* Add `+[MMKV defaultMMKVWithCryptKey:]`, you can encrypt the default MMKV instance now, just like the Android users who already enjoy this for a long time.
* Rename `-[MMKV getValueSizeForKey:]` to `-[MMKV getValueSizeForKey: actualSize:]` to align with Android interface.

### Android
* Fix a potential crash when getting MMKV instances in multi-thread at the same time.
* Add `MMKV.version()` method.

## v1.2.4 / 2020-10-21
This is a hotfix mainly for iOS.

### iOS / macOS
* Fix a decode error of encrypted MMKV on some devices.

### Android
* Fix a potential issue on checking `rootDir` in multi-thread while MMKV initialization is not finished.

## v1.2.3 / 2020-10-16
### Changes for All platforms
* Fix a potential crash on 32-bit devices due to pointer alignment issue.
* Fix a decode error of encrypted MMKV on some 32-bit devices.

### iOS / macOS
* Fix a potential `crc32()` crash on some kind of arm64 devices.
* Fix a potential crash after calling `+[MMKV onAppTerminate]`.

### Android
* Fix a rare lock conflict on `checkProcessMode()`.

### POSIX
Add MMKV support for **Python** on POSIX platforms.  Most things actually work!  
Check out the [wiki](https://github.com/Tencent/MMKV/wiki/python_setup) for more info.

## v1.2.2 / 2020-07-30

### iOS / macOS
* Add auto clean up feature. Call `+[MMKV enableAutoCleanUp:]` to enable auto cleanup MMKV instances that not been accessed recently.
* Fix a potential crash on devices under iPhone X.

### Android
* Add multi-process mode check. After so many issues had been created due to mistakenly using MMKV in multi-process mode in Android, this check is added. If an MMKV instance is accessed with `SINGLE_PROCESS_MODE` in multi-process, an `IllegalArgumentException` will be thrown.

### POSIX
* Add support for armv7 & arm64 arch on Linux.

## v1.2.1 / 2020-07-03
This is a hotfix release. Anyone who has upgraded to v1.2.0 should upgrade to this version **immediately**.

* Fix a potential file corruption bug when writing a file that was created in versions older than v1.2.0. This bug was introduced in v1.2.0.
* Add a preprocess directive `MMKV_DISABLE_CRYPT` to turn off MMKV encryption ability once and for all. If you integrate MMKV by source code, and if you are pretty sure encryption is not needed, you can turn that off to save some binary size.
* The parameter `relativePath` (customizing a separate folder for an MMKV instance), has been renamed to `rootPath`. Making it clear that an absolute path is expected for that parameter.

### iOS / macOS
* `-[MMKV mmkvWithID: relativePath:]` is deprecated. Use `-[MMKV mmkvWithID: rootPath:]` instead. 
* Likewise, `-[MMKV mmkvWithID: cryptKey: relativePath:]` is deprecated. Use `-[MMKV mmkvWithID: cryptKey: rootPath:]` instead. 

## v1.2.0 / 2020-06-30
This is the second **major version** of MMKV. Everything you call is the same as the last version, while almost everything underneath has been improved. 

* **Reduce Memory Footprint**. We used to cache all key-values in a dictionary for efficiency. From now on we store the offset of each key-value inside the mmap-memory instead, **reducing memory footprint by almost half** for (non-encrypt) MMKV. And the accessing efficiency is almost the same as before. As for encrypted MMKV, we can't simply store the offset of each key-value, the relative encrypt info needs to be stored as well. That will be too much for small key-values. We only store such info for large key-values (larger than 256B).
* **Improve Writeback Efficiency**. Thanks to the optimization above, we now can implement writeback by simply calling **memmove()** multiple times. Efficiency is increased and memory consumption is down.
* **Optimize Small Key-Values**. Small key-values of encrypted MMKV are still cached in memory, as all the old versions did. From now on, the struct `MMBuffer` will try to **store small values in the stack** instead of in the heap, saving a lot of time from `malloc()` & `free()`. In fact, all primitive types will be store in the stack memory.

All of the improvements above are available to all supported platforms. Here are the additional changes for each platform.

### iOS / macOS
* **Optimize insert & delete**. Especially for inserting new values to **existing keys**, or deleting keys. We now use the UTF-8 encoded keys in the mmap-memory instead of live encoding from keys, cutting the cost of string encoding conversion.
* Fix Xcode compile error on some projects.
* Drop the support of iOS 8. `thread_local` is not available on iOS 8. We choose to drop support instead of working around because iOS 8's market share is considerably small.

### POSIX
* It's known that GCC before 5.0 doesn't support C++17 standard very well. You should upgrade to the latest version of GCC to compile MMKV.

## v1.1.2 / 2020-05-29

### Android / iOS & macOS / Win32 / POSIX

* Fix a potential crash after `trim()` a multi-process MMKV instance.
* Improve `clearAll()` a bit.

## v1.1.1 / 2020-04-13

### iOS / macOS

* Support WatchOS.
* Rename `+[MMKV onExit]` to `+[MMKV onAppTerminate]`, to avoid naming conflict with some other OpenSource projects.
* Make background write protection much more robust, fix a potential crash when writing meta info in background.
* Fix a potential data corruption bug when writing a UTF-8 (non-ASCII) key.

### Android

* Fix a crash in the demo project when the App is hot reloaded.
* Improve wiki & readme to recommend users to init & destruct MMKV in the `Application` class instead of the `MainActivity` class.

### POSIX
* Fix two compile errors with some compilers.

## v1.1.0 / 2020-03-24
This is the first **major breaking version** ever since MMKV was made public in September 2018, introducing bunches of improvement. Due to the Covid-19, it has been delayed for about a month. Now it's finally here! 

* **Improved File Recovery Strategic**. We store the CRC checksum & actual file size on each sync operation & full write back, plus storing the actual file size in the same file(aka the .crc meta file) as the CRC checksum. Base on our usage inside WeChat on the iOS platform, it cuts the file **corruption rate down by almost half**.
* **Unified Core Library**. We refactor the whole MMKV project and unify the cross-platform Core library. From now on, MMKV on iOS/macOS, Android, Win32 all **share the same core logic code**. It brings many benefits such as reducing the work to fix common bugs, improvements on one platform are available to other platforms immediately, and much more.
* **Supports POSIX Platforms**. Thanks to the unified Core library, we port MMKV to POSIX platforms easily.
* **Multi-Process Access on iOS/macOS**. Thanks to the unified Core library, we add multi-process access to iOS/macOS platforms easily.
* **Efficiency Improvement**. We make the most of armv8 ability including the AES & CRC32 instructions to tune **encryption & error checking speed up by one order higher** than before on armv8 devices. There are bunches of other speed tuning all around the whole project.

Here are the old-style change logs of each platform.

### iOS / macOS
* Adds **multi-process access** support. You should initialize MMKV by calling `+[MMKV initializeMMKV: groupDir: logLevel:]`, passing your **app group id**. Then you can get a multi-process instance by calling `+[MMKV mmkvWithID: mode:]` or `+[MMKV mmkvWithID: cryptKey: mode:]`, accessing it cross your app & your app extensions.
* Add **inter-process content change notification**. You can get MMKV changes notification of other processes by implementing `- onMMKVContentChange:` of `<MMKVHandler>` protocol.
* **Improved File Recovery Strategic**. Cuts the file corruption rate down by almost half. Details are above.
* **Efficiency Improvement**. Encryption & error checking speed are up by one order higher on armv8 devices(aka iDevice including iPhone 5S and above). Encryption on armv7 devices is improved as well. Details are ahead.
* Other speed improvements. Refactor core logic using **MRC**, improve std::vector `push_back()` speed by using **move constructors** & move assignments.
* `+[MMKV setMMKVBasePath:]` & `+[MMKV setLogLevel:]` are marked **deprecated**. You should use `+[MMKV initializeMMKV:]` or `+[MMKV initializeMMKV: logLevel:]` instead.
* The `MMKVLogLevel` enum has been improved in Swift. It can be used like `MMKVLogLevel.info` and so on.

### Android
* **Improved File Recovery Strategic**. Cuts the file corruption rate down by almost half. Details are above.
* **Efficiency Improvement**. Encryption & error checking speed are up by one order higher on armv8 devices with the `arm64-v8a` abi. Encryption on `armeabi` & `armeabi-v7a` is improved as well. Details are ahead.
* Add exception inside core encode & decode logic, making MMKV much more robust.
* Other speed improvements. Improve std::vector `push_back()` speed by using **move constructors** & move assignments.

### Win32
* **Improved File Recovery Strategic**. Cuts the file corruption rate down by almost half. Details are above.
* Add exception inside core encode & decode logic, making MMKV much more robust.
* Other speed improvements. Improve std::vector `push_back()` speed by using **move constructors** & move assignments.

### POSIX
* Most things actually work! We have tested MMKV on the latest version of Linux(Ubuntu, Arch Linux, CentOS, Gentoo), and Unix(macOS, FreeBSD, OpenBSD) on the time v1.1.0 is released.

## v1.0.24 / 2020-01-16

### iOS / macOS
What's new  

* Fix a bug that MMKV will fail to save any key-values after calling `-[MMKV clearMemoryCache]` and then `-[MMKV clearAll]`.
* Add `+[MMKV initializeMMKV:]` for users to init MMKV in the main thread, to avoid an iOS 13 potential crash when accessing `UIApplicationState` in child threads.
* Fix a potential crash when writing a uniquely constructed string.
* Fix a performance slow down when acquiring MMKV instances too often.
* Make the baseline test in MMKVDemo more robust to NSUserDefaults' caches.

### Android
What's new  

* Fix `flock()` bug on ashmem files in Android.
* Fix a potential crash when writing a uniquely constructed string.
* Fix a bug that the MMKVDemo might crash when running in a simulator.

### Win32
* Fix a potential crash when writing a uniquely constructed string or data.

## v1.0.23 / 2019-09-03

### iOS / macOS
What's new  

* Fix a potential security leak on encrypted MMKV.

### Android
What's new  

* Fix a potential security leak on encrypted MMKV.
* Fix filename bug when compiled on Win32 environment.
* Add option for decoding String Set into other `Set<>` classes other than the default `HashSet<String>`, check `decodeStringSet()` for details.
* Add `putBytes()` & `getBytes()`, to make function names more clear and consistent.
* Add notification of content changed by other process, check the new `MMKVContentChangeNotification<>` interface & `checkContentChangedByOuterProcess()` for details.

### Win32
What's new  

* Fix a potential security leak on encrypted MMKV.
* Fix `CriticalSection` init bug.

## v1.0.22 / 2019-06-10

### iOS / macOS
What's new  

* Fix a bug that MMKV will corrupt while adding just one key-value, and reboot or clear memory cache. This bug was introduced in v1.0.21.

### Android
What's new  

* Fix a bug that MMKV will corrupt while adding just one key-value, and reboot or clear memory cache. This bug was introduced in v1.0.21.

### Win32
What's new  

* Fix a bug that MMKV will corrupt while adding just one key-value, and reboot or clear memory cache. This bug was introduced in v1.0.21.

## v1.0.21 / 2019-06-06
### iOS / macOS
What's new  

* Fix a bug that MMKV might corrupt while repeatedly adding & removing key-value with specific length. This bug was introduced in v1.0.20.

### Android
What's new  

* Fix a bug that MMKV might corrupt while repeatedly adding & removing key-value with specific length. This bug was introduced in v1.0.20.

### Win32
What's new  

* Fix a bug that MMKV might corrupt while repeatedly adding & removing key-value with specific length. This bug was introduced in v1.0.20.

## v1.0.20 / 2019-06-05
### iOS / macOS
What's new  

* Fix a bug that MMKV might crash while storing key-value with specific length.
* Fix a bug that `-[MMKV trim]` might not work properly.

### Android
What's new  

* Migrate to AndroidX library.
* Fix a bug that MMKV might crash while storing key-value with specific length.
* Fix a bug that `trim()` might not work properly.
* Fix a bug that dead-lock might be reported by Android mistakenly.
* Using `RegisterNatives()` to simplify native method naming.

### Win32
* Fix a bug that MMKV might crash while storing key-value with specific length.
* Fix a bug that `trim()` might not work properly.
* Fix a bug that `clearAll()` might not work properly.

## v1.0.19 / 2019-04-22
### iOS / macOS
What's new  

* Support Swift 5.
* Add method to get all keys `-[MMKV allKeys]`;
* Add method to synchronize to file asynchronously `-[MMKV async]`.
* Fix a pod configuration bug that might override target project's C++ setting on `CLANG_CXX_LANGUAGE_STANDARD`.
* Fix a bug that `DEFAULT_MMAP_SIZE` might not be initialized before getting any MMKV instance.
* Fix a bug that openssl's header files included inside MMKV might mess with target project's own openssl implementation.

### Android
What's new  

* Support Android Q.
* Add method to synchronize to file asynchronously `void sync()`, or `void apply()` that comes with `SharedPreferences.Editor` interface.
* Fix a bug that a buffer with length of zero might be returned when the key is not existed.
* Fix a bug that `DEFAULT_MMAP_SIZE` might not be initialized before getting any MMKV instance.


## v1.0.18 / 2019-03-14
### iOS / macOS
What's new  

* Fix a bug that defaultValue was not returned while decoding a `NSCoding` value.
* Fix a compile error on static linking MMKV while openssl is static linked too.

### Android
What's new  

* Introducing **Native Buffer**. Checkout [wiki](https://github.com/Tencent/MMKV/wiki/android_advance#native-buffer) for details.
* Fix a potential crash when trying to recover data from file length error.
* Protect from mistakenly passing `Context.MODE_MULTI_PROCESS` to init MMKV.


### Win32
* Fix a potential crash when trying to recover data from file length error.

## v1.0.17 / 2019-01-25
### iOS / macOS
What's new  

* Redirect logging of MMKV is supported now.
* Dynamically disable logging of MMKV is supported now.
* Add method `migrateFromUserDefaults ` to import from NSUserDefaults.

### Android
What's new  

* Redirect logging of MMKV is supported now.
* Dynamically disable logging of MMKV is supported now.  
  Note: These two are breaking changes for interface `MMKVHandler`, update your implementation with `wantLogRedirecting()` & `mmkvLog()` for v1.0.17. (Interface with default method requires API level 24, sigh...)
* Add option to use custom library loader `initialize(String rootDir, LibLoader loader)`. If you're facing `System.loadLibrary()` crash on some low API level device, consider using **ReLinker** to load MMKV. Example can be found in **mmkvdemo**.
* Fix a potential corruption of meta file on multi-process mode.
* Fix a potential crash when the meta file is not valid on multi-process mode.


### Win32
* Redirect logging of MMKV is supported now.
* Dynamically disable logging of MMKV is supported now.
* Fix a potential corruption of meta file on multi-process mode.

## v1.0.16 / 2019-01-04
### iOS / macOS
What's new  

* Customizing root folder of MMKV is supported now.
* Customizing folder for specific MMKV is supported now.
* Add method `getValueSizeForKey:` to get value's size of a key.

### Android
What's new  

* Customizing root folder of MMKV is supported now.
* Customizing folder for specific MMKV is supported now.
* Add method `getValueSizeForKey()` to get value's size of a key.
* Fix a potential crash when the meta file is not valid.


### Win32
MMKV for Windows is released now. Most things actually work!

## v1.0.15 / 2018-12-13
### iOS / macOS
What's new  

* Storing **NSString/NSData/NSDate** directly by calling `setString`/`getSring`, `setData`/`getData`, `setDate`/`getDate`.
* Fix a potential crash due to divided by zero.


### Android
What's new  

* Fix a stack overflow crash due to the **callback** feature introduced by v1.0.13.
* Fix a potential crash due to divided by zero.

### Win32
MMKV for Win32 in under construction. Hopefully will come out in next release. For those who are interested, check out branch `dev_win32` for the latest development.

## v1.0.14 / 2018-11-30
### iOS / macOS
What's new  

* Setting `nil` value to reset a key is supported now.
* Rename `boolValue(forKey:)` to `bool(forKey:)` for Swift.


### Android
What's new  

* `Parcelable` objects can be stored directly into MMKV now.
* Setting `null` value to reset a key is supported now.
* Fix an issue that MMKV's file size might expand unexpectly large in some case.
* Fix an issue that MMKV might crash on multi-thread removing and getting on the same key.


## v1.0.13 / 2018-11-09
### iOS / macOS
What's new  

* Special chars like `/` are supported in MMKV now. The file name of MMKV with special mmapID will be encoded with md5 and stored in seperate folder.
* Add **callback** for MMKV error handling. You can make MMKV to recover instead of discard when crc32 check fail happens.
* Add `trim` and `close` operation. Generally speaking they are not necessary in daily usage. Use them if you worry about disk / memory / fd usage.
* Fix an issue that MMKV's file size might expand unexpectly large in some case.

Known Issues

* Setting `nil` value to reset a key will be ignored. Use `remove` instead.

### Android
What's new  

* Add static linked of libc++ to trim AAR size. Use it when there's no other lib in your App embeds `libc++_shared.so`. Or if you already have an older version of `libc++_shared.so` that doesn't agree with MMKV.  
Add `implementation 'com.tencent:mmkv-static:1.0.13'` to your App's gradle setting to integrate.
* Special chars like `/` are supported in MMKV now. The file name of MMKV with special mmapID will be encoded with md5 and stored in seperate folder.
* Add **callback** for MMKV error handling. You can make MMKV to recover instead of discard when crc32 check fail happens.
* Add `trim` and `close` operation. Generally speaking they are not necessary in daily usage. Use them if you worry about disk / memory / fd usage.

Known Issues

* Setting `null` value to reset a key will be ignored. Use `remove` instead.
* MMKV's file size might expand unexpectly large in some case.

## v1.0.12 / 2018-10-18
### iOS / macOS
What's new  

* Fix `mlock` fail on some devices
* Fix a performance issue caused by mistakenly merge of test code
* Fix CocoaPods integration error of **macOS**

### Android / 2018-10-24
What's new  

* Fix `remove()` causing data inconsistency on `MULTI_PROCESS_MODE`


## v1.0.11 / 2018-10-12
### iOS / macOS
What's new  

* Port to **macOS**
* Support **NSCoding**  
You can  store NSArray/NSDictionary or any object what implements `<NSCoding>` protocol.
* Redesign Swift interface
* Some performance improvement

Known Issues

* MMKV use mmapID as its filename, so don't contain any `/` inside mmapID.
* Storing a value of `type A` and getting by `type B` may not work. MMKV does type erasure while storing values. That means it's hard for MMKV to do value-type-checking, if not impossible.

### Android 
What's new  

* Some performance improvement

Known Issues

* Getting an MMKV instance with mmapID that contains `/` may fail.  
MMKV uses mmapID as its filename, so don't contain any `/` inside mmapID.
* Storing a value of `type A` and getting by `type B` may not work.  
MMKV does type erasure while storing values. That means it's hard for MMKV to do value-type-checking, if not impossible.
* `registerOnSharedPreferenceChangeListener` not supported.  
This is intended. We believe doing data-change-listener inside a storage framework smells really bad to us. We suggest using something like event-bus to notify any interesting clients.

## v1.0.10 / 2018-09-21  

 * Initial Release
