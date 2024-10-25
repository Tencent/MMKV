# MMKV for Flutter Change Log
## v2.0.1 / 2024-10-25
* Fix breaking changes on platform interface package.

## v1.3.10 / 2024-10-25
* Rollback some breaking changes on platform interface package.

## v2.0.0 / 2024-10-21
* Support read-only mode.
* Add add log/error/content-change callback for Flutter & ArtTS
* Bump Android minSdkVersion to 23.
* Drop 32-bit arch support.

## v1.3.9 / 2024-07-26
This will be **the last LTS release of MMKV for Flutter**.
* Modify the dependency of native lib in a way that no Dart package update is needed for any LTS release in the future.
* Fix a data corruption bug on an encrypted MMKV with only one key value stored.
* Make encryption more resilient from brute force cracking.
* Fix a bug that pthread_mutex is not being destroyed correctly.
* Android: Use an alternative way to get the process name to avoid potential App review issues.
* Android: Upgrade to NDK 26.3.11579264.

## v1.3.8 / 2024-07-12
* Add support for **HarmonyOS NEXT**.

## v1.3.7 / 2024-07-08
**Sync with Latest Android Native Binary:**

This Long Term Support (LTS) release primarily reintroduces support for the ARMv7 architecture and lowers the minimum SDK version requirement to 21. Please note that only critical bug fixes will be applied to the 1.3.x series. New features will be introduced in version 2.0 and later, which will discontinue support for 32-bit architectures and raise the minimum SDK version requirement to 23.

## v1.3.6 / 2024-07-05
* Android: MMKV will try to load libmmkv.so before Dart code, to reduce the error of loading library in Android.
* Android: Use the latest ashmem API if possible.
* Android: Use the latest API to get the device API level.

## v1.3.5 / 2024-04-24
* Migrate to federated plugins to avoid the iOS rename headache. From now on, no more renaming from `mmkv` to `mmkvflutter` is needed.
* Bump iOS Deployment Target to iOS 12.
* Bump Android minSdkVersion to 23.

## v1.3.4 / 2024-03-15
* Make `trim()` more robust in multi-process mode.

## v1.3.3 / 2024-01-25
* Add `removeStorage()` static method to safely delete underlying files of an MMKV instance.
* Add protection from a potential crash of a multi-process MMKV loading due to the MMKV file not being valid.
* Add back the lazy load feature. It was first introduced in v1.2.16. But it was rollbacked in v1.3.0 of potential ANR & file corruption. Now it's clear that the bug was caused by something else, it's time to bring it back.
* **Optimize loading speed** by using shared inter-process lock unless there's a need to truncate the file size, which is rare.
* Make these two lately added features **more robust**: customizing the initial file size & optimizing write speed when there's only one key inside MMKV.
* On the Xcode 15 build, an App will crash on iOS 14 and below. Previously we have recommended some workarounds (check the v1.3.2 release note for details). Now you can use Xcode 15.1 to fix this issue.

## v1.3.2 / 2023-11-20
Among most of the features added in this version, the credit goes to @kaitian521.

* Add the feature of customizing the **initial file size** of an MMKV instance.
* **Optimize write speed** when there's only one key inside MMKV, the new key is the same as the old one, and MMKV is in `SINGLE_PROCESS_MODE`.
* **Optimize write speed** by overriding from the beginning of the file instead of append in the back, when there's zero key inside MMKV, and MMKV is in `SINGLE_PROCESS_MODE`.
* Add the feature of `clearAll()` with keeping file disk space unchanged, **reducing the need to expand file size** on later insert & update operations. This feature is off by default, you will have to call it with relative params or newly added methods. 
* Add the feature of **comparing values before setting/encoding** on the same key.
* Fix a potential bug that the MMKV file will be invalid state after a successful expansion but a failure `zeroFill()`, will lead to a crash.
* Fix a potential crash due to other module/static lib turn-off **RTTI**, which will cause MMKV to fail to catch `std::exception`.
* Fix several potential crash due to the MMKV file not being valid.
* Android: Use the `-O2` optimization level by default, which will **reduce native lib size** and improve read/write speed a little bit.
* Android: Experimantal use `@fastNative` annotation on `enableCompareBeforeCompare()` to speed up JNI call.
* Turn-off mlock() protection in background on iOS 13+. We have **verified it on WeChat** that the protection is no longer needed from at least iOS 13. Maybe iOS 12 or older is also not needed, but we don't have the chance to verify that because WeChat no longer supports iOS 12.

#### Known Issue
* On Xcode 15 build, App will crash on iOS 14 and below. The bug is introduced by Apple's new linker. The official solutions provided by Apple are either:
  * Drop the support of iOS 14.
  * Add `-Wl,-weak_reference_mismatches,weak` or `-Wl,-ld_classic` options to the `OTHER_LDFLAGS` build setting of Xcode 15. Note that these options are **not recognized** by older versions of Xcode.
  * Use older versions of Xcode, or **wait for Xcode 15.2**.

## v1.3.1 / 2023-8-11
This is a hotfix version. It's **highly recommended** that v1.2.16 & v1.3.0 users upgrade as soon as possible.
* Fix a critical bug that might cause multi-process MMKV corrupt. This bug was introduced in v1.2.16.
* Add the ability to filter expired keys on `count()` & `allKeys()` methods when auto key expiration is turn on.
* Reduce the `msync()` call on newly created MMKV instances.

## v1.3.0 / 2023-06-14
* Add auto key expiration feature. Note that this is a breaking change, once upgrade to auto expiration, the MMKV file is not valid for older versions of MMKV (v1.2.16 and below) to correctly operate.
* Roll back the lazy load optimization due to reported ANR issues. It was introduced in v1.2.16.
* The version is now the same as the MMKV native library.
* Starting from v1.3.0, Flutter for Android will use `com.tencent:mmkv`. Previously it's `com.tencent:mmkv-static`. It's the same as `com.tencent:mmkv` starting from v1.2.11.

## v1.2.17 / 2023-04-20
* Optimization: The actual file content is lazy loaded now, saving time on MMKV instance creation, and avoiding lock waiting when a lot of instances are created at the same time.
* Fix a bug when restoring a loaded MMKV instance the meta file might mistakenly report corrupted.
* Fix a crash on decoding an empty list.
* Remove deprecated dependence.
* Make the script more robust to fix the iOS Flutter plugin name.
* Keep up with MMKV native lib v1.2.16.

## v1.2.16 / 2023-01-12
* Reduce the privacy info needed to obtain android sdkInt, avoid unnecessary risk on Android App Review.
* Log handler now handles all logs from the very beginning, especially the logs in initialization.
* Log handler register method is now deprecated. It's integrated with initialize().
* Keep up with MMKV native lib v1.2.15.

## v1.2.15 / 2022-08-10
* Fix a bug that `MMKV.decodeXXX()` may return invalid results in multi-process mode.
* Upgrade to Flutter 3.0.
* Keep up with MMKV native lib v1.2.14.

## v1.2.14 / 2022-03-30
* Replace the deprecated `device_info` package with `device_info_plus`.
* Keep up with MMKV native lib v1.2.13.

## v1.2.13 / 2022-01-17
* Fix a bug that a subsequential `clearAll()` call may fail to take effect in multi-process mode.
* Hide some OpenSSL symbols to prevent link-time symbol conflict, when an App somehow also static linking OpenSSL.
* Upgrade Android `compileSdkVersion` & `targetSdkVersion` from `30` to `31`.
* Keep up with MMKV native lib v1.2.12.

## v1.2.12 / 2021-10-26
* Add backup & restore ability.
* Keep up with MMKV native lib v1.2.11.

## v1.2.11 / 2021-06-25
* Bug Fixed: When building on iOS, occasionally it will fail on symbol conflict with other libs. We have renamed all public native methods to avoid potential conflict.
* Keep up with MMKV native lib v1.2.10.

## v1.2.10 / 2021-05-26
* Bug Fixed: When calling `MMKV.encodeString()` with an empty string value on Android, `MMKV.decodeString()` will return `null`.
* Bug Fixed: After upgrading from Flutter 1.20+ to 2.0+, calling `MMKV.defaultMMKV()` on Android might fail to load, you can try calling `MMKV.defaultMMKV(cryptKey: '\u{2}U')` instead.
* Keep up with MMKV native lib v1.2.9, which drops the **armeabi** arch on Android.

## v1.2.9 / 2021-05-06
* Support null-safety.
* Upgrade to Flutter 2.0.
* Keep up with MMKV native lib v1.2.8, which migrates the Android Native Lib to Maven Central Repository.
* Fix `MMKV.encodeString()` crash on iOS with an empty string value.

### Known Issue
* When calling `MMKV.encodeString()` with an empty string value on Android, `MMKV.decodeString()` will return `null`. This bug will be fixed in the next version of Android Native Lib. iOS does not have such a bug.

## v1.2.8 / 2020-12-25
* Keep up with MMKV native lib v1.2.7, which fix the `MMKV.sync(false)` not being asynchronous bug.
* Fix `MMKV.defaultMMKV()` crash on iOS simulator.
* Fix `MMKV.defaultMMKV(cryptKey)` not encrypted as expected bug on iOS.

## v1.2.7 / 2020-11-27
Fix iOS symbol not found bug.

## v1.2.6 / 2020-11-27
The first official flutter plugin of MMKV. Most things actually work!
