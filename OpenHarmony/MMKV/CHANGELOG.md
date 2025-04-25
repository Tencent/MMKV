# MMKV for HarmonyOS NEXT Change Log
## v2.2.1 / 2025-04-25
* Add `importFrom()`.

## v2.2.0 / 2025-04-24
We introduce the **Mayfly FD** (short-lived file descriptor) enhancement, reducing MMKV's fd footprint by half and more. For a single-process mode MMKV instance, the fd footprint is reduced to zero (except Android/OHOS, details below). For a multi-process mode MMKV instance, the fd footprint is reduced by half, for we still need a long-lived fd to inter-process lock the shared memory.
* Add **Mayfly FD** (short-lived file descriptor) enhancement. Reduce the fd footprint by half. For a single-process mode MMKV instance, we still need a long-lived fd to support the **legacy name upgrading**. In the far future, when all legacy names have been upgraded, we might drop the long-lived fd like other platforms.
* Improve multi-process access efficiency by about 20%.
* Add `checkExist()` to check if a MMKV instance exists on disk.
* Drop deprecated armv7 AES hardware acceleration.
* Drop `checkProcessMode()`, it’s never been used.
* Improve obfuscation configuration with relative path.

## v2.1.0 / 2025-02-18
* **Breaking change**: Migrate legacy MMKV in a custom directory to normal MMKV. Historically Android/OHOS mistakenly use mmapKey as mmapID, which will be problematic with the `NameSpace` feature. Starting from v2.1.0, MMKV will try to migrate them back to normal when possible.  
  It's highly recommended that you **upgrade to v2.0.2 first** with **forward support** of normal MMKV in a custom directory.
* Supports using MMKV directly in C++ code.
* Improve inter-process locking by using `F_OFD_SETLK` instead of `F_SETLK`.
* Improve directory creation on `ReadOnly` mode.
* Add *experimental* protection from bad disk records of MMKV files.
* Fix FileLock not being unlocked on destruction.

## v2.0.2 / 2024-12-27
* Obfuscation fully supported.
* Use atomic file rename on OHOS.
* Add forward support for the correct filename with a custom root path.

## v2.0.1 / 2024-11-12
* Fix a bug that MMKV might become dead-locked for other threads after `decodeStringSet()` / `decodeNumberSet` / `decodeBoolSet` or decoding `TypedArray`.

## v2.0.0 / 2024-10-21
* Support obfuscation. For the time being, you will have to manually copy the content of MMKV's [consumer-rules.txt](https://github.com/Tencent/MMKV/blob/master/OpenHarmony/MMKV/consumer-rules.txt) into your App's obfuscation-rules.txt.
* Support read-only mode.
* Add add log/error/content-change callback for Flutter & ArtTS

## v1.3.9 / 2024-07-26
* Fix a data corruption bug on an encrypted MMKV with only one key value stored.
* Make encryption more resilient from brute force cracking.

## v1.3.7 / 2024-07-12
* Add support of Flutter on HarmonyOS NEXT.

## v1.3.6 / 2024-07-05
* Fix a bug that a `String` value might get truncated on encoding.
* MMKV returns `undefined` when a key does not exist, previously a default value of the type (`false` for `boolean`, `0` for `number`, etc) is returned.
* Add the feature to encode/decode a `float` value.
* Add the feature to encode/decode a `TypedArray` value.
* Support encoding a part of an `ArrayBuffer`.

## v1.3.5 / 2024-04-24
Fix a bug in `MMKV.initialize()` that fails to handle the optional parameter _logLevel_.

## v1.3.4 / 2024-04-17

The first official HarmonyOS NEXT package of MMKV. Most things actually work!
