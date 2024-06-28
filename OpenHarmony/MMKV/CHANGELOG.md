# MMKV for HarmonyOS NEXT Change Log

## v1.3.6-beta3 / 2024-06-28
* Add encode / decode `float` interface.
* Add encode / decode all `TypedArray` interface.
* Fix a bug when transforming a JavaScript string to a native string, the last char might lost.
* Fix a bug when decoding an non-existent value of primitive types, `undefined` is not returned.
* Fix a bug when encoding a TypedArray with only a view of a ArrayBuffer, it will not be encoded correctly.

## v1.3.5 / 2024-04-24
Fix a bug in `MMKV.initialize()` that fails to handle the optional parameter _logLevel_.

## v1.3.4 / 2024-04-17

The first official HarmonyOS NEXT package of MMKV. Most things actually work!
