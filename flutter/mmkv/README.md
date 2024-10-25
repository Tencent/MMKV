[![license](https://img.shields.io/badge/license-BSD_3-brightgreen.svg?style=flat)](https://github.com/Tencent/MMKV/blob/master/LICENSE.TXT)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/MMKV/pulls)
[![Release Version](https://img.shields.io/badge/release-2.0.1-brightgreen.svg)](https://github.com/Tencent/MMKV/releases)
[![Platform](https://img.shields.io/badge/Platform-%20Android%20%7C%20iOS-brightgreen.svg)](https://github.com/Tencent/MMKV/wiki/home)

MMKV is an **efficient**, **small**, **easy-to-use** mobile key-value storage framework used in the WeChat application. It's currently available on **Android** and **iOS**.

# MMKV for Flutter

## Features

* **Efficient**. MMKV uses mmap to keep memory synced with file, and protobuf to encode/decode values, making the most of native platform to achieve best performance.
  * **Multi-Process concurrency**: MMKV supports concurrent read-read and read-write access between processes.

* **Easy-to-use**. You can use MMKV as you go. All changes are saved immediately, no `sync`, no `apply` calls needed.

* **Small**.
  * **A handful of files**: MMKV contains process locks, encode/decode helpers and mmap logics and nothing more. It's really tidy.
  * **About 100K in binary size**: MMKV adds about 100K per architecture on App size, and much less when zipped (apk/ipa).


## Getting Started

### Installation
Add the following lines to `pubspec.yaml` on your app module. Then run `flutter pub get`.

```yaml
dependencies:
  mmkv: "^2.0.1"
```

If you already include MMKV native lib in your App, you need to upgrade to version newer than v2.0.0.  

#### iOS  
Starting from v1.3.5, there's **no need** to change the plugin name 'mmkv' to 'mmkvflutter'. You should remove the script (`fix_mmkv_plugin_name()`) previously added in your Podfile. 

#### Android  
If you previously use `com.tencent.mmkv-static` or `com.tencent.mmkv-shared` in your Android App, you should move to `com.tencent.mmkv`.
And if your App depends on any 3rd SDK that embeds `com.tencent.mmkv-static` or `com.tencent.mmkv-shared`, you can add this lines to your `build.gradle` to avoid conflict:

```gradle
    dependencies {
        ...

        modules {
            module("com.tencent:mmkv-static") {
                replacedBy("com.tencent:mmkv", "Using mmkv for flutter")
            }
            module("com.tencent:mmkv-shared") {
                replacedBy("com.tencent:mmkv", "Using mmkv for flutter")
            }
        }
    }
```

### Setup
You can use MMKV as you go. All changes are saved immediately, no `sync`, no `apply` calls needed.  
Setup MMKV on App startup, say your `main()` function, add these lines:

```dart
import 'package:mmkv/mmkv.dart';

void main() async {

  // must wait for MMKV to finish initialization
  final rootDir = await MMKV.initialize();
  print('MMKV for flutter with rootDir = $rootDir');

  runApp(MyApp());
}
```
Note that you have to **wait for MMKV to finish initialization** before accessing any MMKV instance.

### CRUD Operations

* MMKV has a global instance, that can be used directly:

    ```dart
    import 'package:mmkv/mmkv.dart';
        
    var mmkv = MMKV.defaultMMKV();
    mmkv.encodeBool('bool', true);
    print('bool = ${mmkv.decodeBool('bool')}');
    
    mmkv.encodeInt32('int32', (1<<31) - 1);
    print('max int32 = ${mmkv.decodeInt32('int32')}');
    
    mmkv.encodeInt('int', (1<<63) - 1);
    print('max int = ${mmkv.decodeInt('int')}');
    
    String str = 'Hello Flutter from MMKV';
    mmkv.encodeString('string', str);
    print('string = ${mmkv.decodeString('string')}');

    str = 'Hello Flutter from MMKV with bytes';
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(str))!;
    mmkv.encodeBytes('bytes', bytes);
    bytes.destroy();

    bytes = mmkv.decodeBytes('bytes')!;
    print('bytes = ${Utf8Decoder().convert(bytes.asList()!)}');
    bytes.destroy();
    ```

    As you can see, MMKV is quite easy to use.
    
    **Note**: If you come across to failing to load `defaultMMKV()` **on Android** after **upgrading** Flutter from 1.20+ to 2.0+, you can try passing this encryption key `'\u{2}U'` instead.  

   ```dart
   var mmkv = MMKV.defaultMMKV(cryptKey: '\u{2}U');
   ```
    
* **Deleting & Querying**:

    ```dart
    var mmkv = MMKV.defaultMMKV();

    mmkv.removeValue('bool');
    print('contains "bool": ${mmkv.containsKey('bool')}');

    mmkv.removeValues(['int32', 'int']);
    print('all keys: ${mmkv.allKeys}');
    ```

* If different modules/logic need **isolated storage**, you can also create your own MMKV instance separately:

    ```dart
    var mmkv = MMKV("test");
    mmkv.encodeBool('bool', true);
    print('bool = ${mmkv.decodeBool('bool')}');
    ```

* If **multi-process accessing** is needed，you can set `MMKV.MULTI_PROCESS_MODE` on MMKV initialization:

    ```dart
    var mmkv = MMKV("test-multi-process", mode: MMKVMode.MULTI_PROCESS_MODE);
    mmkv.encodeBool('bool', true);
    print('bool = ${mmkv.decodeBool('bool')}');
    ```

### Supported Types
* Primitive Types:
  - `bool, int, double`

* Classes & Collections:
  - `String, List<int>, MMBuffer`

### Log

* By default, MMKV prints log to logcat/console, which is not convenient for diagnosing online issues. 
You can setup MMKV **log redirecting** on App startup on the **native** interface of MMKV. 
Checkout how to do it on [Android](https://github.com/Tencent/MMKV/wiki/android_advance#log) / [iOS](https://github.com/Tencent/MMKV/wiki/ios_advance#log).
Due to the current limitation of Flutter runtime, we can't redirect log on the Flutter side.

* You can turn off MMKV's logging once and for all on initialization (which we strongly disrecommend).  

    ```dart
    final rootDir = await MMKV.initialize(logLevel: MMKVLogLevel.None);
    ```
### Encryption
* By default MMKV stores all key-values in plain text on file, relying on Android's/iOS's sandbox to make sure the file is encrypted. Should you worry about information leaking, you can choose to encrypt MMKV.

    ```dart
    final encryptKey = 'MyEncryptKey';
    var mmkv = MMKV("test-encryption", cryptKey: encryptKey);
    ```

* You can change the encryption key later as you like. You can also change an existing MMKV instance from encrypted to unencrypted, or vice versa.

    ```dart
    // an unencrypted MMKV instance
    var mmkv = MMKV("test-encryption");

    // change from unencrypted to encrypted
    mmkv.reKey("Key_seq_1");

    // change encryption key
    mmkv.reKey("Key_seq_2");

    // change from encrypted to unencrypted
    kmmkv.reKey(null);
    ```
 
### Customize location
* By default, MMKV stores file inside `$(FilesDir)/mmkv/`. You can customize MMKV's **root directory** on App startup:

    ```dart
    final dir = await getApplicationSupportDirectory();
    final rootDir = await MMKV.initialize(dir, rootDir: dir.path + '/mmkv_2');
    print('MMKV for flutter with rootDir = $rootDir');
    ```

* You can even customize any MMKV instance's location:

    ```dart
    final dir = await getApplicationSupportDirectory();
    var mmkv = MMKV('testCustomDir', rootDir: dir.path + '/mmkv_3');
    ```
  **Note:** It's recommended to store MMKV files **inside** your App's sandbox path. **DO NOT** store them on external storage(aka SD card). If you have to do it, you should  follow Android's [scoped storage](https://developer.android.com/preview/privacy/storage) enforcement.

### Additional docs
For additional documents, checkout the [wiki](https://github.com/Tencent/MMKV/wiki/flutter_setup).

## License
MMKV is published under the BSD 3-Clause license. For details check out the [LICENSE.TXT](https://github.com/Tencent/MMKV/blob/master/LICENSE.TXT).

## Change Log
Check out the [CHANGELOG.md](https://github.com/Tencent/MMKV/blob/master/flutter/CHANGELOG.md) for details of change history.

## Contributing

If you are interested in contributing, check out the [CONTRIBUTING.md](https://github.com/Tencent/MMKV/blob/master/CONTRIBUTING.md), also join our [Tencent OpenSource Plan](https://opensource.tencent.com/contribution).

To give clarity of what is expected of our members, MMKV has adopted the code of conduct defined by the Contributor Covenant, which is widely used. And we think it articulates our values well. For more, check out the [Code of Conduct](https://github.com/Tencent/MMKV/blob/master/CODE_OF_CONDUCT.md).

## FAQ & Feedback
Check out the [FAQ](https://github.com/Tencent/MMKV/wiki/FAQ) first. Should there be any questions, don't hesitate to create [issues](https://github.com/Tencent/MMKV/issues).
