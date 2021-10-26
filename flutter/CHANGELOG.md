# MMKV for Flutter Change Log

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
