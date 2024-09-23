[![license](https://img.shields.io/badge/license-BSD_3-brightgreen.svg?style=flat)](https://github.com/Tencent/MMKV/blob/master/LICENSE.TXT)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/MMKV/pulls)
[![Release Version](https://img.shields.io/badge/release-2.0.0-brightgreen.svg)](https://github.com/Tencent/MMKV/releases)
[![Platform](https://img.shields.io/badge/Platform-%20HarmonyOS%20NEXT-brightgreen.svg)](https://github.com/Tencent/MMKV/wiki/home)

MMKV is an **efficient**, **small**, **easy-to-use** mobile key-value storage framework used in the WeChat application. It's now available on **HarmonyOS NEXT**.

# MMKV for HarmonyOS NEXT

## Features

* **Efficient**. MMKV uses mmap to keep memory synced with file, and protobuf to encode/decode values, making the most of native platform to achieve best performance.
  * **Multi-Process concurrency**: MMKV supports concurrent read-read and read-write access between processes.

* **Easy-to-use**. You can use MMKV as you go. All changes are saved immediately, no `sync`, no `flush` calls needed.

* **Small**.
  * **A handful of files**: MMKV contains process locks, encode/decode helpers and mmap logics and nothing more. It's really tidy.
  * **About 600K in binary size**: MMKV adds about 600K per architecture on App size, and much less when zipped (HAR/HAP).


## Getting Started

### Prerequisites

* Apps using MMKV can target: HarmonyOS Next (3.0.0.13) or later (API 12).
* ARM64 & x86_64 architecture.
* DevEco Studio NEXT Developer Beta1 (5.0.3.100) or later.

### Installation via OHPM:
This is the fastest and recommended way to add MMKV to your project.
```bash
ohpm install @tencent/mmkv
```
Or, you can add it to your project manually.
* Add the following lines to `oh-package.json5` on your app module.

  ```json
  "dependencies": {
      "@tencent/mmkv": "^2.0.0",
  }
  ```
* Then run
 
  ```bash
  ohpm install
  ```

For additional installation methods, checkout the [wiki](https://github.com/Tencent/MMKV/wiki/ohos_setup#installation).

### Setup
You can use MMKV as you go. All changes are saved immediately, no `sync`, no `apply` calls needed.  
Setup MMKV on App startup, say your `EntryAbility.onCreate()` function, add these lines:

```js
import { MMKV } from '@tencent/mmkv';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    let appCtx = this.context.getApplicationContext();
    let mmkvRootDir = MMKV.initialize(appCtx);
    console.info('mmkv rootDir: ', mmkvRootDir);
    ……
  }
```

### CRUD Operations

* MMKV has a global instance, that can be used directly:

    ```js
    import { MMKV } from '@tencent/mmkv';
        
    let mmkv = MMKV.defaultMMKV();
    mmkv.encodeBool('bool', true);
    console.info('bool = ', mmkv.decodeBool('bool'));
    
    mmkv.encodeInt32('int32', Math.pow(2, 31) - 1);
    console.info('max int32 = ', mmkv.decodeInt32('int32'));
    
    mmkv.encodeInt64('int', BigInt(2**63) - BigInt(1));
    console.info('max int64 = ', mmkv.decodeInt64('int'));
    
    let str: string = 'Hello OpenHarmony from MMKV';
    mmkv.encodeString('string', str);
    console.info('string = ', mmkv.decodeString('string'));

    let arrayBuffer: ArrayBuffer = StringToArrayBuffer('Hello OpenHarmony from MMKV with bytes');
    mmkv.encodeBytes('bytes', arrayBuffer);
    let bytes = mmkv.decodeBytes('bytes');
    console.info('bytes = ', ArrayBufferToString(bytes));
    ```

    As you can see, MMKV is quite easy to use.
    
* **Deleting & Querying**:

    ```js
    mmkv.removeValueForKey('bool');
    console.info('contains "bool"', mmkv.containsKey('bool'));

    mmkv.removeValuesForKeys(['int32', 'int']);
    console.info('all keys: ', mmkv.allKeys().join());
    ```

* If different modules/logic need **isolated storage**, you can also create your own MMKV instance separately:

    ```js
    var mmkv = MMKV.mmkvWithID('test');
    mmkv.encodeBool('bool', true);
    console.info('bool = ', mmkv.decodeBool('bool'));
    ```

* If **multi-process accessing** is needed，you can set `MMKV.MULTI_PROCESS_MODE` on MMKV initialization:

    ```dart
    var mmkv = MMKV.mmkvWithID('test-multi-process', MMKV.MULTI_PROCESS_MODE);
    mmkv.encodeBool('bool', true);
    console.info('bool = ', mmkv.decodeBool('bool'));
    ```

### Supported Types
* Primitive Types:
  - `boolean, number, bigint, string`

* Classes & Collections:
  - `boolean[], number[], string[], ArrayBuffer, TypedArray`

### Log

* By default, MMKV prints log to hilog, which is not convenient for diagnosing online issues. 
You can setup MMKV **log redirecting** on App startup on the **native** interface of MMKV. 
Checkout how to do it on [C++](https://github.com/Tencent/MMKV/wiki/posix_tutorial#logs).
Due to the current limitation of NAPI runtime, we **can't efficiently** redirect log to the JavaScript side.

* You can turn off MMKV's logging once and for all on initialization (which we strongly disrecommend).  

    ```js
    import { MMKV, MMKVLogLevel } from '@tencent/mmkv';

    MMKV.initialize(appCtx, MMKVLogLevel.None);
    ```

### Encryption
* By default MMKV stores all key-values in plain text on file, relying on Android's/iOS's sandbox to make sure the file is encrypted. Should you worry about information leaking, you can choose to encrypt MMKV.

    ```js
    let encryptKey = 'MyEncryptKey';
    let mmkv = MMKV.mmkvWithID('test-encryption', MMKV.SINGLE_PROCESS_MODE, encryptKey);
    ```

* You can change the encryption key later as you like. You can also change an existing MMKV instance from encrypted to unencrypted, or vice versa.

    ```js
    // an unencrypted MMKV instance
    let mmkv = MMKV.mmkvWithID('test-encryption');

    // change from unencrypted to encrypted
    mmkv.reKey('Key_seq_1');

    // change encryption key
    mmkv.reKey('Key_seq_2');

    // change from encrypted to unencrypted
    kmmkv.reKey();
    ```
 
### Customize location
* By default, MMKV stores file inside `$(FilesDir)/mmkv/`. You can customize MMKV's **root directory** on App startup:

    ```js
    let appCtx = this.context.getApplicationContext();
    let rootDir = appCtx.filesDir + '/mmkv_2';
    let cacheDir = appCtx.cacheDir;
    MMKV.initializeWithPath(rootDir, cacheDir);
    ```

* You can even customize any MMKV instance's location:

    ```js
    let appCtx = this.context.getApplicationContext();
    let rootDir = appCtx.filesDir + '/mmkv_3';
    var mmkv = MMKV.mmkvWithID('testCustomDir', MMKV.SINGLE_PROCESS_MODE, null, rootDir);
    ```
  **Note:** It's recommended to store MMKV files **inside** your App's sandbox path. **DO NOT** store them on external storage(aka SD card).

### Additional docs
For additional documents, checkout the [wiki](https://github.com/Tencent/MMKV/wiki/ohos_setup).

## License
MMKV is published under the BSD 3-Clause license. For details check out the [LICENSE.TXT](https://github.com/Tencent/MMKV/blob/master/LICENSE.TXT).

## Change Log
Check out the [CHANGELOG.md](https://github.com/Tencent/MMKV/blob/master/OpenHarmony/MMKV/CHANGELOG.md) for details of change history.

## Contributing

If you are interested in contributing, check out the [CONTRIBUTING.md](https://github.com/Tencent/MMKV/blob/master/CONTRIBUTING.md), also join our [Tencent OpenSource Plan](https://opensource.tencent.com/contribution).

To give clarity of what is expected of our members, MMKV has adopted the code of conduct defined by the Contributor Covenant, which is widely used. And we think it articulates our values well. For more, check out the [Code of Conduct](https://github.com/Tencent/MMKV/blob/master/CODE_OF_CONDUCT.md).

## FAQ & Feedback
Check out the [FAQ](https://github.com/Tencent/MMKV/wiki/FAQ) first. Should there be any questions, don't hesitate to create [issues](https://github.com/Tencent/MMKV/issues).
