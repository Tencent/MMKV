# MMKV for HarmonyOS NEXT Change Log

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
