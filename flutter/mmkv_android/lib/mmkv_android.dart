import "dart:ffi";
import "package:ffi/ffi.dart";
import "package:flutter/services.dart";
import "package:path_provider/path_provider.dart";
import "package:mmkv_platform_interface/mmkv_platform_interface.dart";

final class MMKVPlatformAndroid extends MMKVPluginPlatform {
  // A default real implementation of the platform interface would go here.
  static void registerWith() {
    MMKVPluginPlatform.instance = MMKVPlatformAndroid();
  }

  @pragma("vm:prefer-inline")
  static String _nativeFuncName(String name) {
    return name;
  }

  static const MethodChannel _channel = MethodChannel("mmkv");

  static final _nativeLib = DynamicLibrary.open("libmmkv.so");

  static final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeBool =
  _nativeLib.lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>(_nativeFuncName("encodeBool")).asFunction();
  static final void Function(Pointer<Utf8> rootDir, Pointer<Utf8> cacheDir, int sdkInt, int logLevel) _mmkvInitialize = _nativeLib.lookup<NativeFunction<Void Function(Pointer<Utf8>, Pointer<Utf8>, Int32, Int32)>>("mmkvInitialize_v1").asFunction();

  @override
  Future<String> initialize(String rootDir, {String? groupDir, int logLevel = 1}) async {
    final rootDirPtr = _string2Pointer(rootDir);
    final sdkInt = await _channel.invokeMethod("getSdkVersion") ?? 0;
    final cacheDir = await getTemporaryDirectory();
    final cacheDirPtr = _string2Pointer(cacheDir.path);

    _mmkvInitialize(rootDirPtr, cacheDirPtr, sdkInt, logLevel);

    calloc.free(rootDirPtr);
    calloc.free(cacheDirPtr);
    return rootDir;
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeBoolFunc()  {
    return _encodeBool;
  }
}

Pointer<Utf8> _string2Pointer(String? str) {
  if (str != null) {
    return str.toNativeUtf8();
  }
  return nullptr;
}
