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
import "package:flutter/services.dart";
// import "package:path_provider/path_provider.dart";
// import "package:path_provider_ohos/path_provider_ohos.dart";
import "package:mmkv_platform_interface/mmkv_platform_interface.dart";

// final
class MMKVPlatformOHOS extends MMKVPluginPlatformFFI {
  // A default real implementation of the platform interface would go here.
  static void registerWith() {
    MMKVPluginPlatform.instance = MMKVPlatformOHOS();
  }

  static final _nativeLib = DynamicLibrary.open("libmmkv.so");

  @override
  DynamicLibrary nativeLib() {
    return _nativeLib;
  }

  static const MethodChannel _channel = MethodChannel("mmkv");

  @override
  Future<String> initialize(String rootDir, {String? groupDir, int logLevel = 1, Pointer<NativeFunction<LogCallbackWrap>>? logHandler}) async {
    final Map<String, dynamic> params = {
      "rootDir": rootDir,
      "logLevel": logLevel,
    };
    final cacheDir = await getTemporaryPath();
    params["cacheDir"] = cacheDir;
    if (logHandler != null) {
      print("warn: Flutter on OHOS does not support log redirecting! You should use the ArtTS interface instead.");
    }

    final ret = await _channel.invokeMethod("initializeMMKV", params);
    return ret;
  }

  /// OHOS doesn't publish their path_provider package to pub.dev
  /// we got no choice but to override these calls
  /// to make the dependency on them as less as possible

  // static final _pathProviderImp = PathProviderOhos();

  @override
  Future<String> getApplicationDocumentsPath() async {
    // final result = await getApplicationDocumentsDirectory();
    // return result.path;
    // final result = await _pathProviderImp.getApplicationDocumentsPath();
    // return result!;
    final result = await _channel.invokeMethod("getApplicationDocumentsPath");
    return result;
  }

  @override
  Future<String> getTemporaryPath() async {
    // final result = await getTemporaryDirectory();
    // return result.path;
    // final result = await _pathProviderImp.getTemporaryPath();
    // return result!;
    final result = await _channel.invokeMethod("getTemporaryPath");
    return result;
  }
}
