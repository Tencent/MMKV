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

// final
class MMKVPlatformIOS extends MMKVPluginPlatformFFI {
  static void registerWith() {
    MMKVPluginPlatform.instance = MMKVPlatformIOS();
  }

  @override
  String nativeFuncName(String name) {
    return "mmkv_$name";
  }

  static late final _nativeLib = DynamicLibrary.process();

  @override
  DynamicLibrary nativeLib() {
    return _nativeLib;
  }

  static const MethodChannel _channel = MethodChannel("mmkv");

  static final Pointer<Utf8> Function(Pointer<Utf8> rootDir, Pointer<Utf8> groupDir, int logLevel, Pointer<NativeFunction<LogCallbackWrap>>)
      _mmkvInitialize = _nativeLib
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Utf8>, Pointer<Utf8>, Int32, Pointer<NativeFunction<LogCallbackWrap>>)>>(
              "mmkvInitialize")
          .asFunction();

  @override
  Future<String> initialize(String rootDir, {String? groupDir, int logLevel = 1, Pointer<NativeFunction<LogCallbackWrap>>? logHandler}) async {
    final rootDirPtr = _string2Pointer(rootDir);
    final groupDirPtr = groupDir != null ? _string2Pointer(groupDir) : nullptr;

    final ret = _mmkvInitialize(rootDirPtr, groupDirPtr, logLevel, logHandler ?? nullptr);

    calloc.free(rootDirPtr);
    calloc.free(groupDirPtr);

    return _pointer2String(ret) ?? rootDir;
  }
}

Pointer<Utf8> _string2Pointer(String? str) {
  if (str != null) {
    return str.toNativeUtf8();
  }
  return nullptr;
}

String? _pointer2String(Pointer<Utf8>? ptr) {
  if (ptr != null && ptr != nullptr) {
    return ptr.toDartString();
  }
  return null;
}
