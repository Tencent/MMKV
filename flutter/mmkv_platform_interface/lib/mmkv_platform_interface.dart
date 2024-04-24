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

export "mmkv_platform_ffi.dart";

/// The interface class that all implementation of MMKV platform plugin must extend
abstract base class MMKVPluginPlatform {
  MMKVPluginPlatform();

  // A plugin can have a default implementation, as shown here, or `instance`
  // can be nullable, and the default instance can be null.
  static MMKVPluginPlatform? instance = null;

  // Methods for the plugin's platform interface would go here, often with
  // implementations that throw UnimplementedError.

  Future<String> initialize(String rootDir, {String? groupDir, int logLevel = 1}) async {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeBoolFunc() {
    throw UnimplementedError();
  }

  Pointer<Void> Function(Pointer<Utf8> mmapID, int, Pointer<Utf8> cryptKey, Pointer<Utf8> rootDir, int expectedCapacity) getMMKVWithIDFunc() {
    throw UnimplementedError();
  }

  Pointer<Void> Function(int, Pointer<Utf8> cryptKey) getDefaultMMKVFunc() {
    throw UnimplementedError();
  }

  Pointer<Utf8> Function(Pointer<Void>) mmapIDFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int, int) encodeBoolV2Func() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int) decodeBoolFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeInt32Func() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int, int) encodeInt32V2Func() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int) decodeInt32Func() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeInt64Func() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int, int) encodeInt64V2Func() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int) decodeInt64Func() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, double) encodeDoubleFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, double, int) encodeDoubleV2Func() {
    throw UnimplementedError();
  }

  double Function(Pointer<Void>, Pointer<Utf8>, double) decodeDoubleFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int) encodeBytesFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int, int) encodeBytesV2Func() {
    throw UnimplementedError();
  }

  Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint64>) decodeBytesFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Uint8>, int) reKeyFunc() {
    throw UnimplementedError();
  }

  Pointer<Uint8> Function(Pointer<Void>, Pointer<Uint64>) cryptKeyFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>, Pointer<Uint8>, int) checkReSetCryptKeyFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, int) valueSizeFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>, int) writeValueToNBFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>, Pointer<Pointer<Uint32>>, int) allKeysFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, Pointer<Utf8>) containsKeyFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>, int) countFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>) totalSizeFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Void>) actualSizeFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>, Pointer<Utf8>) removeValueForKeyFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>, Pointer<Pointer<Utf8>>, Pointer<Uint32>, int) removeValuesForKeysFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>, int) clearAllFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>, int) mmkvSyncFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>) clearMemoryCacheFunc() {
    throw UnimplementedError();
  }

  int Function() pageSizeFunc() {
    throw UnimplementedError();
  }

  Pointer<Utf8> Function() versionFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>) trimFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>) mmkvCloseFunc() {
    throw UnimplementedError();
  }

  void Function(Pointer<Void>, Pointer<Void>, int) memcpyFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Utf8> mmapID, Pointer<Utf8> dstDir, Pointer<Utf8> rootPath) backupOneFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Utf8> mmapID, Pointer<Utf8> srcDir, Pointer<Utf8> rootPath) restoreOneFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Utf8> dstDir) backupAllFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Utf8> srcDir) restoreAllFunc() {
    throw UnimplementedError();
  }

  bool Function(Pointer<Void>, int) enableAutoExpireFunc() {
    throw UnimplementedError();
  }

  bool Function(Pointer<Void>) disableAutoExpireFunc() {
    throw UnimplementedError();
  }

  bool Function(Pointer<Void>) enableCompareBeforeSetFunc() {
    throw UnimplementedError();
  }

  bool Function(Pointer<Void>) disableCompareBeforeSetFunc() {
    throw UnimplementedError();
  }

  int Function(Pointer<Utf8> mmapID, Pointer<Utf8> rootPath) removeStorageFunc() {
    throw UnimplementedError();
  }
}
