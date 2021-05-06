# MMKV for Flutter Change Log

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
