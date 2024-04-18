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

import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart';
import 'package:mmkv_platform_interface/mmkv_platform_interface.dart';

final class MMKVPlatformIOS extends MMKVPluginPlatform {
  static void registerWith() {
    MMKVPluginPlatform.instance = MMKVPlatformIOS();
  }

  static String _nativeFuncName(String name) {
    return "mmkv_$name";
  }

  static const MethodChannel _channel = MethodChannel("mmkv");

  static late final _nativeLib = DynamicLibrary.process();

  static late final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeBool =
  _nativeLib.lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>(_nativeFuncName("encodeBool")).asFunction();

  @override
  Future<String> initialize(String rootDir, {String? groupDir, int logLevel = 1}) async {
    final Map<String, dynamic> params = {
      "rootDir": rootDir,
      "logLevel": logLevel,
    };
    if (groupDir != null) {
      params["groupDir"] = groupDir;
    }
    final ret = await _channel.invokeMethod("initializeMMKV", params);
    return ret;
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeBoolFunc()  {
    return _encodeBool;
  }
}
