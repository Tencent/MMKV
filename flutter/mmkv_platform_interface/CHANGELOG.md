# MMKV Platform Interface Change Log
## v2.4.0 / 2026-03-18
* Add `onMMKVContentLoadSuccessfully` to `MMKVHandler`.
* Add `MMKVConfig` and `defaultMMKV(config)` support.

## v2.3.0 / 2025-12-03
* Alter `getMMKVWithID2()`, `getDefaultMMKV()`, `reKey()` and `checkReSetCryptKey()` to support **AES-256 encryption** functionality.

## v2.2.3 / 2025-08-20
* Protect from `freePtr()` not found.

## v2.2.2 / 2025-08-20
* Add `freePtr()`, mainly for Windows.

## v2.2.1 / 2025-4-25
* Add `importFrom()`.

## v2.2.0 / 2025-4-24
* Add `checkExist()`.
* Add `groupPath()` for iOS.
* Add `isFileValid()`.

## v2.1.0 / 2025-02-18
* Bump to 2.1 to setup a breaking change version.
* Add `getNameSpace()`.

## v2.0.0 / 2024-10-25
Bump to 2.0 to setup a breaking change version.

## v1.0.3 / 2024-10-24
* Rollback `isMultiProcess()`.
* Rollback `isReadOnly()`.

## v1.0.2 / 2024-10-21
* Add `isMultiProcess()`.
* Add `isReadOnly()`.

## v1.0.1 / 2024-07-12
* Add override point for path_provider.

## v1.0.0 / 2024-04-19
The initial release.
