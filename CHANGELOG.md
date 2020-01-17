# MMKV Change Log

## v1.0.24 / 2020-01-16

### iOS / macOS
What's new  

* Fix a bug that MMKV will fail to save any key-values after calling `-[MMKV clearMemoryCache]` and then `-[MMKV clearAll]`.
* Add `-[MMKV initializeMMKV:]` for users to init MMKV in the main thread, to avoid an iOS 13 potential crash when accessing `UIApplicationState` in child threads.
* Fix a potential crash when writing a uniquely constructed string.
* Fix a performance slow down when acquiring MMKV instances too often.
* Make the baseline test in MMKVDemo more robust to NSUserDefaults' caches.

### Android
What's new  

* Fix flock() bug on ashmem files in Android.
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

* Getting a MMKV instance with mmapID that contains `/` may fail.  
MMKV uses mmapID as its filename, so don't contain any `/` inside mmapID.
* Storing a value of `type A` and getting by `type B` may not work.  
MMKV does type erasure while storing values. That means it's hard for MMKV to do value-type-checking, if not impossible.
* `registerOnSharedPreferenceChangeListener` not supported.  
This is intended. We believe doing data-change-listener inside a storage framework smells really bad to us. We suggest using something like event-bus to notify any interesting clients.

## v1.0.10 / 2018-09-21  

 * Initial Release
