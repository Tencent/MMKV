/*
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
 * Licensed under the BSD 3-Clause License.
 */

import "dart:ffi";
import "dart:io";

import "package:ffi/ffi.dart";
import "package:mmkv/mmkv.dart";
import "package:mmkv_platform_interface/mmkv_platform_interface.dart";
import "package:test/test.dart";

// Native integration test, with no mocked MMKV operations. Supply a desktop
// Flutter bridge library built against this checkout, and local package overrides:
// MMKV_TEST_LIBRARY=/path/to/library flutter test test/native_expire_time_test.dart
// Set MMKV_TEST_APPLE_BRIDGE=1 for the Objective-C iOS/macOS bridge ABI.
class _NativeTestPlatform extends MMKVPluginPlatformFFI {
  _NativeTestPlatform(this.library, this.appleBridge);

  final DynamicLibrary library;
  final bool appleBridge;

  @override
  DynamicLibrary nativeLib() => library;

  @override
  String nativeFuncName(String name) => appleBridge ? "mmkv_$name" : name;

  @override
  Future<String> initialize(String rootDir,
      {String? groupDir,
      int logLevel = 1,
      Pointer<NativeFunction<LogCallbackWrap>>? logHandler}) async {
    final root = rootDir.toNativeUtf8();
    try {
      if (appleBridge) {
        library.lookupFunction<
                Pointer<Void> Function(
                    Pointer<Utf8>, Pointer<Utf8>, Int32, Pointer<Void>),
                Pointer<Void> Function(
                    Pointer<Utf8>, Pointer<Utf8>, int, Pointer<Void>)>(
            "mmkvInitialize")(root, nullptr, logLevel, nullptr);
      } else {
        library.lookupFunction<
            Pointer<Void> Function(Pointer<Utf8>, Int32, Pointer<Void>),
            Pointer<Void> Function(Pointer<Utf8>, int,
                Pointer<Void>)>("mmkvInitialize")(root, logLevel, nullptr);
      }
    } finally {
      calloc.free(root);
    }
    return rootDir;
  }
}

int _nowSeconds() => DateTime.now().millisecondsSinceEpoch ~/ 1000;

void main() {
  final libraryPath = Platform.environment["MMKV_TEST_LIBRARY"];
  group("native expiration timestamp", () {
    late Directory root;
    late MMKV kv;
    var sequence = 0;
    late String mmapID;

    setUpAll(() async {
      MMKVPluginPlatform.instance = _NativeTestPlatform(
          DynamicLibrary.open(libraryPath!),
          Platform.environment["MMKV_TEST_APPLE_BRIDGE"] == "1");
      final parent =
          Directory(Platform.environment["MMKV_TEST_ROOT"] ?? ".dart_tool");
      parent.createSync(recursive: true);
      root = parent.createTempSync("mmkv-expire-").absolute;
      await MMKV.initialize(rootDir: root.path);
    });

    setUp(() {
      mmapID = "expire_${sequence++}";
      kv = MMKV(mmapID, rootDir: root.path, enableKeyExpire: false);
    });

    tearDown(() {
      kv.close();
      expect(MMKV.removeStorage(mmapID, rootDir: root.path), isTrue);
    });

    tearDownAll(() => root.deleteSync(recursive: true));

    test("disabled, missing, never, and closed return zero", () {
      expect(kv.encodeBool("disabled", true), isTrue);
      expect(kv.getExpireTimeForKey("disabled"), 0);
      expect(kv.enableAutoKeyExpire(60), isTrue);
      expect(kv.getExpireTimeForKey("missing"), 0);
      expect(kv.getExpireTimeForKey(""), 0);
      expect(kv.encodeBool("never", true, MMKV.ExpireNever), isTrue);
      expect(kv.getExpireTimeForKey("never"), 0);
      kv.close();
      expect(kv.getExpireTimeForKey("never"), 0);
    });

    test(
        "default and explicit durations produce persistent absolute timestamps",
        () {
      expect(kv.enableAutoKeyExpire(60), isTrue);
      final before = _nowSeconds();
      expect(kv.encodeBool("default", true), isTrue);
      expect(kv.encodeBool("explicit", true, 120), isTrue);
      final after = _nowSeconds();
      final defaultTime = kv.getExpireTimeForKey("default");
      final explicitTime = kv.getExpireTimeForKey("explicit");
      expect(defaultTime, inInclusiveRange(before + 60, after + 60));
      expect(explicitTime, inInclusiveRange(before + 120, after + 120));
      kv.clearMemoryCache();
      expect(kv.getExpireTimeForKey("default"), defaultTime);
      kv.close();
      kv = MMKV(mmapID, rootDir: root.path);
      expect(kv.getExpireTimeForKey("explicit"), explicitTime);
      kv.removeValue("explicit");
      expect(kv.getExpireTimeForKey("explicit"), 0);
      expect(kv.disableAutoKeyExpire(), isTrue);
      expect(kv.getExpireTimeForKey("default"), 0);
    });

    test("maximum uint32 timestamp remains positive", () {
      expect(kv.enableAutoKeyExpire(0), isTrue);
      expect(kv.encodeBool("unsigned", true, 0xffffffff), isTrue);
      expect(kv.getExpireTimeForKey("unsigned"), 0xffffffff);
    });

    test("expired timestamp remains until a read removes the key", () async {
      expect(kv.enableAutoKeyExpire(0), isTrue);
      expect(kv.encodeBool("expired", true, 1), isTrue);
      final timestamp = kv.getExpireTimeForKey("expired");
      expect(timestamp, greaterThan(0));
      await Future<void>.delayed(const Duration(seconds: 2));
      expect(timestamp, lessThanOrEqualTo(_nowSeconds()));
      expect(kv.getExpireTimeForKey("expired"), timestamp);
      expect(kv.decodeBool("expired"), isFalse);
      expect(kv.getExpireTimeForKey("expired"), 0);
    });
  },
      skip: libraryPath == null
          ? "Set MMKV_TEST_LIBRARY to a native desktop Flutter bridge library."
          : false);
}
