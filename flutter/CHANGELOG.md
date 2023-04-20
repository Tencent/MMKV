# MMKV for Flutter Change Log

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
