# MMKV for HarmonyOS NEXT Change Log
## v1.3.12 / 2025-01-02
* Fix a bug that MMKV might fail to backup/restore across different filesystems.
* Add protection from invalid value size of auto-key-expire mmkv.
* Add forward support for the correct filename with a custom root path.
* Obfuscation fully supported.

## v1.3.11 / 2024-11-12
* Fix a bug that after encode / decode `TypedArray`, the instance become dead-locked for other threads.

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
