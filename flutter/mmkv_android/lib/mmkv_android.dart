/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import "dart:ffi";
import "package:ffi/ffi.dart";
import "package:flutter/services.dart";
import "package:path_provider/path_provider.dart";
import "package:mmkv_platform_interface/mmkv_platform_interface.dart";

final class MMKVPlatformAndroid extends MMKVPluginPlatformFFI {
  // A default real implementation of the platform interface would go here.
  static void registerWith() {
    MMKVPluginPlatform.instance = MMKVPlatformAndroid();
  }

  static final _nativeLib = DynamicLibrary.open("libmmkv.so");

  @override
  DynamicLibrary nativeLib() {
    return _nativeLib;
  }

  static const MethodChannel _channel = MethodChannel("mmkv");

  static final void Function(Pointer<Utf8> rootDir, Pointer<Utf8> cacheDir, int sdkInt, int logLevel) _mmkvInitialize =
      _nativeLib.lookup<NativeFunction<Void Function(Pointer<Utf8>, Pointer<Utf8>, Int32, Int32)>>("mmkvInitialize_v1").asFunction();

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
}

Pointer<Utf8> _string2Pointer(String? str) {
  if (str != null) {
    return str.toNativeUtf8();
  }
  return nullptr;
}
