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
import "mmkv_platform_interface.dart";

/// A helper class to ease the implementation of MMKV platform plugin in FFI
// abstract base
class MMKVPluginPlatformFFI extends MMKVPluginPlatform {
  /// tells which dylib to lookup for function pointer
  DynamicLibrary nativeLib() {
    throw UnimplementedError();
  }

  /// a chance to map native function name (to avoid potential conflict)
  String nativeFuncName(String name) {
    return name;
  }

  @override
  Pointer<Void> Function(Pointer<Utf8> mmapID, int, Pointer<Utf8> cryptKey, Pointer<Utf8> rootDir, int expectedCapacity) getMMKVWithIDFunc() {
    return nativeLib()
        .lookup<NativeFunction<Pointer<Void> Function(Pointer<Utf8>, Uint32, Pointer<Utf8>, Pointer<Utf8>, Uint32)>>("getMMKVWithID")
        .asFunction();
  }

  @override
  Pointer<Void> Function(int, Pointer<Utf8> cryptKey) getDefaultMMKVFunc() {
    return nativeLib().lookup<NativeFunction<Pointer<Void> Function(Uint32, Pointer<Utf8>)>>("getDefaultMMKV").asFunction();
  }

  @override
  Pointer<Utf8> Function(Pointer<Void>) mmapIDFunc() {
    return nativeLib().lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(nativeFuncName("mmapID")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeBoolFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>(nativeFuncName("encodeBool")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int, int) encodeBoolV2Func() {
    return nativeLib()
        .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8, Uint32)>>(nativeFuncName("encodeBool_v2"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) decodeBoolFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>(nativeFuncName("decodeBool")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeInt32Func() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int32)>>(nativeFuncName("encodeInt32")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int, int) encodeInt32V2Func() {
    return nativeLib()
        .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int32, Uint32)>>(nativeFuncName("encodeInt32_v2"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) decodeInt32Func() {
    return nativeLib().lookup<NativeFunction<Int32 Function(Pointer<Void>, Pointer<Utf8>, Int32)>>(nativeFuncName("decodeInt32")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) encodeInt64Func() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int64)>>(nativeFuncName("encodeInt64")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int, int) encodeInt64V2Func() {
    return nativeLib()
        .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int64, Uint32)>>(nativeFuncName("encodeInt64_v2"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) decodeInt64Func() {
    return nativeLib().lookup<NativeFunction<Int64 Function(Pointer<Void>, Pointer<Utf8>, Int64)>>(nativeFuncName("decodeInt64")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, double) encodeDoubleFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Double)>>(nativeFuncName("encodeDouble")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, double, int) encodeDoubleV2Func() {
    return nativeLib()
        .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Double, Uint32)>>(nativeFuncName("encodeDouble_v2"))
        .asFunction();
  }

  @override
  double Function(Pointer<Void>, Pointer<Utf8>, double) decodeDoubleFunc() {
    return nativeLib().lookup<NativeFunction<Double Function(Pointer<Void>, Pointer<Utf8>, Double)>>(nativeFuncName("decodeDouble")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int) encodeBytesFunc() {
    return nativeLib()
        .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, Uint64)>>(nativeFuncName("encodeBytes"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int, int) encodeBytesV2Func() {
    return nativeLib()
        .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, Uint64, Uint32)>>(nativeFuncName("encodeBytes_v2"))
        .asFunction();
  }

  @override
  Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint64>) decodeBytesFunc() {
    return nativeLib()
        .lookup<NativeFunction<Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint64>)>>(nativeFuncName("decodeBytes"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Uint8>, int) reKeyFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Uint8>, Uint64)>>(nativeFuncName("reKey")).asFunction();
  }

  @override
  Pointer<Uint8> Function(Pointer<Void>, Pointer<Uint64>) cryptKeyFunc() {
    return nativeLib().lookup<NativeFunction<Pointer<Uint8> Function(Pointer<Void>, Pointer<Uint64>)>>(nativeFuncName("cryptKey")).asFunction();
  }

  @override
  void Function(Pointer<Void>, Pointer<Uint8>, int) checkReSetCryptKeyFunc() {
    return nativeLib()
        .lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Uint8>, Uint64)>>(nativeFuncName("checkReSetCryptKey"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, int) valueSizeFunc() {
    return nativeLib().lookup<NativeFunction<Uint32 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>(nativeFuncName("valueSize")).asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>, int) writeValueToNBFunc() {
    return nativeLib()
        .lookup<NativeFunction<Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>, Uint32)>>(nativeFuncName("writeValueToNB"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>, Pointer<Pointer<Uint32>>, int) allKeysFunc() {
    return nativeLib()
        .lookup<NativeFunction<Uint64 Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>, Pointer<Pointer<Uint32>>, Int8)>>(
        nativeFuncName("allKeys"))
        .asFunction();
  }

  @override
  int Function(Pointer<Void>, Pointer<Utf8>) containsKeyFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>)>>(nativeFuncName("containsKey")).asFunction();
  }

  @override
  int Function(Pointer<Void>, int) countFunc() {
    return nativeLib().lookup<NativeFunction<Uint64 Function(Pointer<Void>, Int8)>>(nativeFuncName("count")).asFunction();
  }

  @override
  int Function(Pointer<Void>) totalSizeFunc() {
    return nativeLib().lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>(nativeFuncName("totalSize")).asFunction();
  }

  @override
  int Function(Pointer<Void>) actualSizeFunc() {
    return nativeLib().lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>(nativeFuncName("actualSize")).asFunction();
  }

  @override
  void Function(Pointer<Void>, Pointer<Utf8>) removeValueForKeyFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Utf8>)>>(nativeFuncName("removeValueForKey")).asFunction();
  }

  @override
  void Function(Pointer<Void>, Pointer<Pointer<Utf8>>, Pointer<Uint32>, int) removeValuesForKeysFunc() {
    return nativeLib()
        .lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Pointer<Utf8>>, Pointer<Uint32>, Uint64)>>(nativeFuncName("removeValuesForKeys"))
        .asFunction();
  }

  @override
  void Function(Pointer<Void>, int) clearAllFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>, Uint32)>>(nativeFuncName("clearAll")).asFunction();
  }

  @override
  void Function(Pointer<Void>, int) mmkvSyncFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>, Int8)>>("mmkvSync").asFunction();
  }

  @override
  void Function(Pointer<Void>) clearMemoryCacheFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>)>>(nativeFuncName("clearMemoryCache")).asFunction();
  }

  @override
  int Function() pageSizeFunc() {
    return nativeLib().lookup<NativeFunction<Int32 Function()>>(nativeFuncName("pageSize")).asFunction();
  }

  @override
  Pointer<Utf8> Function() versionFunc() {
    return nativeLib().lookup<NativeFunction<Pointer<Utf8> Function()>>(nativeFuncName("version")).asFunction();
  }

  @override
  void Function(Pointer<Void>) trimFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>)>>(nativeFuncName("trim")).asFunction();
  }

  @override
  void Function(Pointer<Void>) mmkvCloseFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>)>>("mmkvClose").asFunction();
  }

  @override
  void Function(Pointer<Void>, Pointer<Void>, int) memcpyFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Void>, Uint64)>>("mmkvMemcpy").asFunction();
  }

  @override
  int Function(Pointer<Utf8> mmapID, Pointer<Utf8> dstDir, Pointer<Utf8> rootPath) backupOneFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)>>(nativeFuncName("backupOne")).asFunction();
  }

  @override
  int Function(Pointer<Utf8> mmapID, Pointer<Utf8> srcDir, Pointer<Utf8> rootPath) restoreOneFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)>>(nativeFuncName("restoreOne")).asFunction();
  }

  @override
  int Function(Pointer<Utf8> dstDir) backupAllFunc() {
    return nativeLib().lookup<NativeFunction<Uint64 Function(Pointer<Utf8>)>>(nativeFuncName("backupAll")).asFunction();
  }

  @override
  int Function(Pointer<Utf8> srcDir) restoreAllFunc() {
    return nativeLib().lookup<NativeFunction<Uint64 Function(Pointer<Utf8>)>>(nativeFuncName("restoreAll")).asFunction();
  }

  @override
  bool Function(Pointer<Void>, int) enableAutoExpireFunc() {
    return nativeLib().lookup<NativeFunction<Bool Function(Pointer<Void>, Uint32)>>(nativeFuncName("enableAutoExpire")).asFunction();
  }

  @override
  bool Function(Pointer<Void>) disableAutoExpireFunc() {
    return nativeLib().lookup<NativeFunction<Bool Function(Pointer<Void>)>>(nativeFuncName("disableAutoExpire")).asFunction();
  }

  @override
  bool Function(Pointer<Void>) enableCompareBeforeSetFunc() {
    return nativeLib().lookup<NativeFunction<Bool Function(Pointer<Void>)>>(nativeFuncName("enableCompareBeforeSet")).asFunction();
  }

  @override
  bool Function(Pointer<Void>) disableCompareBeforeSetFunc() {
    return nativeLib().lookup<NativeFunction<Bool Function(Pointer<Void>)>>(nativeFuncName("disableCompareBeforeSet")).asFunction();
  }

  @override
  int Function(Pointer<Utf8> mmapID, Pointer<Utf8> rootPath) removeStorageFunc() {
    return nativeLib().lookup<NativeFunction<Int8 Function(Pointer<Utf8>, Pointer<Utf8>)>>(nativeFuncName("removeStorage")).asFunction();
  }

  @override
  bool Function(Pointer<Void>) isMultiProcessFunc() {
    return nativeLib().lookup<NativeFunction<Bool Function(Pointer<Void>)>>(nativeFuncName("isMultiProcess")).asFunction();
  }

  @override
  bool Function(Pointer<Void>) isReadOnlyFunc() {
    return nativeLib().lookup<NativeFunction<Bool Function(Pointer<Void>)>>(nativeFuncName("isReadOnly")).asFunction();
  }

  @override
  ErrorCallbackRegister registerErrorHandlerFunc() {
    return nativeLib().lookup<NativeFunction<ErrorCallbackRegisterWrap>>(nativeFuncName("registerErrorHandler")).asFunction();
  }

  @override
  ContentCallbackRegister registerContentHandlerFunc() {
    return nativeLib().lookup<NativeFunction<ContentCallbackRegisterWrap>>(nativeFuncName("registerContentChangeNotify")).asFunction();
  }

  @override
  void Function(Pointer<Void> p1) checkContentChangedFunc() {
    return nativeLib().lookup<NativeFunction<Void Function(Pointer<Void>)>>(nativeFuncName("checkContentChanged")).asFunction();
  }
}
