/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.
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

import "dart:async";
import "dart:convert";
import "dart:ffi"; // For FFI
import "dart:io"; // For Platform.isX

import "package:ffi/ffi.dart";
import "package:flutter/cupertino.dart";
import "package:flutter/material.dart";
import "package:flutter/services.dart";
import "package:path_provider/path_provider.dart";

/// Log level for MMKV.
enum MMKVLogLevel { Debug, Info, Warning, Error, None }

/// Process mode for MMKV, default to [SINGLE_PROCESS_MODE].
enum MMKVMode {
  INVALID_MODE,
  SINGLE_PROCESS_MODE,
  MULTI_PROCESS_MODE,
}

/// A native memory buffer, must call [MMBuffer.destroy()] after no longer use.
class MMBuffer {
  int _length = 0;

  /// The size of the memory buffer.
  int get length => _length;

  Pointer<Uint8>? _ptr;

  /// The pointer of underlying memory buffer.
  Pointer<Uint8>? get pointer => _ptr;

  /// Create a memory buffer with size of [length].
  MMBuffer(int length) {
    _length = length;
    if (length > 0) {
      _ptr = malloc<Uint8>(length);
    } else {
      _ptr = nullptr;
    }
  }

  /// Copy all data from [list].
  static MMBuffer? fromList(List<int>? list) {
    if (list == null) {
      return null;
    }

    final buffer = MMBuffer(list.length);
    if (list.isEmpty) {
      buffer._ptr = malloc<Uint8>();
    }
    buffer.asList()!.setAll(0, list);
    return buffer;
  }

  /// Create a wrapper of native pointer [ptr] with size [length].
  /// DON'T [destroy()] the result because it's not a copy.
  static MMBuffer _fromPointer(Pointer<Uint8> ptr, int length) {
    final buffer = MMBuffer(0);
    buffer._length = length;
    buffer._ptr = ptr;
    return buffer;
  }

  /// Create a wrapper of native pointer [ptr] with size [length].
  /// DO remember to [destroy()] the result because it's a COPY.
  static MMBuffer _copyFromPointer(Pointer<Uint8> ptr, int length) {
    final buffer = MMBuffer(length);
    buffer._length = length;
    if (length == 0 && ptr != nullptr) {
      buffer._ptr = malloc<Uint8>();
    }
    _memcpy(buffer.pointer!.cast(), ptr.cast(), length);
    return buffer;
  }

  /// Must call this after no longer use.
  void destroy() {
    if (_ptr != null && _ptr != nullptr) {
      malloc.free(_ptr!);
    }
    _ptr = null;
    _length = 0;
  }

  /// Get a **list view** of the underlying data.
  /// Must call [destroy()] later after not longer use.
  Uint8List? asList() {
    if (_ptr != null && _ptr != nullptr) {
      return _ptr!.asTypedList(_length);
    }
    return null;
  }

  /// Copy the underlying data as a list.
  /// And [destroy()] itself at the same time.
  Uint8List? takeList() {
    if (_ptr != null && _ptr != nullptr) {
      final list = Uint8List.fromList(asList()!);
      destroy();
      return list;
    }
    return null;
  }
}

/// An efficient, small mobile key-value storage framework developed by WeChat.
/// Works on Android & iOS.
class MMKV {
  Pointer<Void> _handle = nullptr;
  static String _rootDir = "";

  static const MethodChannel _channel = MethodChannel("mmkv");

  /// MMKV must be initialized before any usage.
  ///
  /// Generally speaking you should do this inside `main()`:
  /// ```dart
  /// void main() async {
  ///   // must wait for MMKV to finish initialization
  ///   final rootDir = await MMKV.initialize();
  ///   print('MMKV for flutter with rootDir = $rootDir');
  ///
  ///   runApp(MyApp());
  /// }
  /// ```
  /// Note that you must **wait for it** to finish before any usage.
  /// * You can customize MMKV's root dir by passing [rootDir], `${Document}/mmkv` by default.
  /// * You can customize MMKV's log level by passing [logLevel].
  /// You can even turnoff logging by passing [MMKVLogLevel.None], which we don't recommend doing.
  /// * If you want to use MMKV in multi-process on iOS, you should set group folder by passing [groupDir].
  /// [groupDir] will be ignored on Android.
  static Future<String> initialize(
      {String? rootDir,
      String? groupDir,
      MMKVLogLevel logLevel = MMKVLogLevel.Info}) async {
    WidgetsFlutterBinding.ensureInitialized();

    if (rootDir == null) {
      final path = await getApplicationDocumentsDirectory();
      rootDir = "${path.path}/mmkv";
    }
    _rootDir = rootDir;

    if (Platform.isIOS) {
      final Map<String, dynamic> params = {
        "rootDir": rootDir,
        "logLevel": logLevel.index,
      };
      if (groupDir != null) {
        params["groupDir"] = groupDir;
      }
      final ret = await _channel.invokeMethod("initializeMMKV", params);
      return ret;
    } else {
      final rootDirPtr = _string2Pointer(rootDir);
      final sdkInt = await _channel.invokeMethod("getSdkVersion") ?? 0;
      final cacheDir = await getTemporaryDirectory();
      final cacheDirPtr = _string2Pointer(cacheDir.path);

      _mmkvInitialize!(rootDirPtr, cacheDirPtr, sdkInt, logLevel.index);

      calloc.free(rootDirPtr);
      calloc.free(cacheDirPtr);
      return rootDir;
    }
  }

  /// The root directory of MMKV.
  static String get rootDir {
    return _rootDir;
  }

  /// A generic purpose instance in single-process mode.
  ///
  /// Note: If you come across to failing to load [defaultMMKV()] on Android after upgrading Flutter from 1.20+ to 2.0+,
  /// you can try passing this [cryptKey] `'\u{2}U'` instead.
  /// ```dart
  /// var mmkv = MMKV.defaultMMKV(cryptKey: '\u{2}U');
  /// ```
  static MMKV defaultMMKV({String? cryptKey}) {
    final mmkv = MMKV("");
    final cryptKeyPtr = _string2Pointer(cryptKey);
    const mode = MMKVMode.SINGLE_PROCESS_MODE;
    mmkv._handle = _getDefaultMMKV(mode.index, cryptKeyPtr);
    calloc.free(cryptKeyPtr);
    return mmkv;
  }

  /// Get an MMKV instance with an unique ID [mmapID].
  ///
  /// * If you want a per-user mmkv, you could merge user-id within [mmapID].
  /// * You can get a multi-process MMKV instance by passing [MMKVMode.MULTI_PROCESS_MODE].
  /// * You can encrypt with [cryptKey], which limits to 16 bytes at most.
  /// * You can customize the [rootDir] of the file.
  MMKV(String mmapID,
      {MMKVMode mode = MMKVMode.SINGLE_PROCESS_MODE,
      String? cryptKey,
      String? rootDir}) {
    if (mmapID.isNotEmpty) {
      final mmapIDPtr = _string2Pointer(mmapID);
      final cryptKeyPtr = _string2Pointer(cryptKey);
      final rootDirPtr = _string2Pointer(rootDir);

      _handle = _getMMKVWithID(mmapIDPtr, mode.index, cryptKeyPtr, rootDirPtr);

      calloc.free(mmapIDPtr);
      calloc.free(cryptKeyPtr);
      calloc.free(rootDirPtr);
    }
  }

  String get mmapID {
    return _pointer2String(_mmapID(_handle))!;
  }

  bool encodeBool(String key, bool value) {
    final keyPtr = key.toNativeUtf8();
    final ret = _encodeBool(_handle, keyPtr, _bool2Int(value));
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  bool decodeBool(String key, {bool defaultValue = false}) {
    final keyPtr = key.toNativeUtf8();
    final ret = _decodeBool(_handle, keyPtr, _bool2Int(defaultValue));
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  /// Use this when the [value] won't be larger than a normal int32.
  /// It's more efficient & cost less space.
  bool encodeInt32(String key, int value) {
    final keyPtr = key.toNativeUtf8();
    final ret = _encodeInt32(_handle, keyPtr, value);
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  /// Use this when the value won't be larger than a normal int32.
  /// It's more efficient & cost less space.
  int decodeInt32(String key, {int defaultValue = 0}) {
    final keyPtr = key.toNativeUtf8();
    final ret = _decodeInt32(_handle, keyPtr, defaultValue);
    calloc.free(keyPtr);
    return ret;
  }

  bool encodeInt(String key, int value) {
    final keyPtr = key.toNativeUtf8();
    final ret = _encodeInt64(_handle, keyPtr, value);
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  int decodeInt(String key, {int defaultValue = 0}) {
    final keyPtr = key.toNativeUtf8();
    final ret = _decodeInt64(_handle, keyPtr, defaultValue);
    calloc.free(keyPtr);
    return ret;
  }

  bool encodeDouble(String key, double value) {
    final keyPtr = key.toNativeUtf8();
    final ret = _encodeDouble(_handle, keyPtr, value);
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  double decodeDouble(String key, {double defaultValue = 0}) {
    final keyPtr = key.toNativeUtf8();
    final ret = _decodeDouble(_handle, keyPtr, defaultValue);
    calloc.free(keyPtr);
    return ret;
  }

  /// Encode an utf-8 string.
  bool encodeString(String key, String? value) {
    if (value == null) {
      removeValue(key);
      return true;
    }

    final keyPtr = key.toNativeUtf8();
    final bytes = MMBuffer.fromList(const Utf8Encoder().convert(value))!;

    final ret = _encodeBytes(_handle, keyPtr, bytes.pointer!, bytes.length);

    calloc.free(keyPtr);
    bytes.destroy();
    return _int2Bool(ret);
  }

  /// Decode as an utf-8 string.
  String? decodeString(String key) {
    final keyPtr = key.toNativeUtf8();
    final lengthPtr = calloc<Uint64>();

    final ret = _decodeBytes(_handle, keyPtr, lengthPtr);
    calloc.free(keyPtr);

    if (ret != nullptr) {
      final length = lengthPtr.value;
      calloc.free(lengthPtr);
      final result = _buffer2String(ret, length);
      if (!Platform.isIOS && length > 0) {
        calloc.free(ret);
      }
      return result;
    }
    calloc.free(lengthPtr);
    return null;
  }

  /// Encoding bytes.
  ///
  /// You can serialize an object into bytes, then store it inside MMKV.
  /// ```dart
  /// // assume using protobuf https://developers.google.com/protocol-buffers/docs/darttutorial
  /// var object = MyClass();
  /// final list = object.writeToBuffer();
  /// final buffer = MMBuffer.fromList(list);
  ///
  /// mmkv.encodeBytes('bytes', buffer);
  ///
  /// buffer.destroy();
  /// ```
  bool encodeBytes(String key, MMBuffer? value) {
    if (value == null) {
      removeValue(key);
      return true;
    }

    final keyPtr = key.toNativeUtf8();
    final ret = _encodeBytes(_handle, keyPtr, value.pointer!, value.length);
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  /// Decoding bytes.
  ///
  /// You can decode bytes from MMKV, then deserialize an object from the bytes.
  /// ```dart
  /// // assume using protobuf https://developers.google.com/protocol-buffers/docs/darttutorial
  /// final bytes = mmkv.decodeBytes('bytes');
  /// if (bytes != null) {
  ///   final list = bytes.asList();
  ///   final object = MyClass.fromBuffer(list);
  ///
  ///   // Must [destroy()] after no longer use.
  ///   bytes.destroy();
  /// }
  /// ```
  MMBuffer? decodeBytes(String key) {
    final keyPtr = key.toNativeUtf8();
    final lengthPtr = calloc<Uint64>();

    final ret = _decodeBytes(_handle, keyPtr, lengthPtr);
    calloc.free(keyPtr);

    if (/*ret != null && */ ret != nullptr) {
      final length = lengthPtr.value;
      calloc.free(lengthPtr);
      if (Platform.isIOS || length == 0) {
        return MMBuffer._copyFromPointer(ret, length);
      } else {
        return MMBuffer._fromPointer(ret, length);
      }
    }
    calloc.free(lengthPtr);
    return null;
  }

  /// Change encryption key for the MMKV instance.
  ///
  /// * The [cryptKey] is 16 bytes limited.
  /// * You can transfer a plain-text MMKV into encrypted by setting an non-null, non-empty [cryptKey].
  /// * Or vice versa by passing [cryptKey] with null.
  /// See also [checkReSetCryptKey()].
  bool reKey(String? cryptKey) {
    if (cryptKey != null && cryptKey.isNotEmpty) {
      final bytes = MMBuffer.fromList(const Utf8Encoder().convert(cryptKey))!;
      final ret = _reKey(_handle, bytes.pointer!, bytes.length);
      bytes.destroy();
      return _int2Bool(ret);
    } else {
      final ret = _reKey(_handle, nullptr, 0);
      return _int2Bool(ret);
    }
  }

  /// See also [reKey()].
  String? get cryptKey {
    final lengthPtr = calloc<Uint64>();
    final ret = _cryptKey(_handle, lengthPtr);
    if (/*ret != null && */ ret != nullptr) {
      final length = lengthPtr.value;
      calloc.free(lengthPtr);
      final result = _buffer2String(ret, length);
      calloc.free(ret);
      return result;
    }
    return null;
  }

  /// Just reset the [cryptKey] (will not encrypt or decrypt anything).
  /// Usually you should call this method after other process [reKey()] the multi-process mmkv.
  void checkReSetCryptKey(String cryptKey) {
    final bytes = MMBuffer.fromList(const Utf8Encoder().convert(cryptKey))!;
    _checkReSetCryptKey(_handle, bytes.pointer!, bytes.length);
    bytes.destroy();
  }

  /// Get the actual size consumption of the key's value.
  /// Pass [actualSize] with true to get value's length.
  int valueSize(String key, bool actualSize) {
    final keyPtr = key.toNativeUtf8();
    final ret = _valueSize(_handle, keyPtr, _bool2Int(actualSize));
    calloc.free(keyPtr);
    return ret;
  }

  /// Write the value to a pre-allocateallocated native buffer.
  ///
  /// * Return size written into buffer.
  /// * Return -1 on any error, such as [buffer] not large enough.
  int writeValueToNativeBuffer(String key, MMBuffer buffer) {
    final keyPtr = key.toNativeUtf8();
    final ret =
        _writeValueToNB(_handle, keyPtr, buffer.pointer!.cast(), buffer.length);
    calloc.free(keyPtr);
    return ret;
  }

  /// Get all the keys (_unsorted_).
  List<String> get allKeys {
    final keyArrayPtr = calloc<Pointer<Pointer<Utf8>>>();
    final sizeArrayPtr = calloc<Pointer<Uint32>>();
    final List<String> keys = [];

    final count = _allKeys(_handle, keyArrayPtr, sizeArrayPtr);
    if (count > 0) {
      final keyArray = keyArrayPtr[0];
      final sizeArray = sizeArrayPtr[0];
      for (int index = 0; index < count; index++) {
        final keyPtr = keyArray[index];
        final size = sizeArray[index];
        final key = _buffer2String(keyPtr.cast(), size);
        if (key != null) {
          keys.add(key);
        }
        if (!Platform.isIOS) {
          calloc.free(keyPtr);
        }
      }
      calloc.free(keyArray);
      calloc.free(sizeArray);
    }

    calloc.free(sizeArrayPtr);
    calloc.free(keyArrayPtr);

    return keys;
  }

  bool containsKey(String key) {
    final keyPtr = key.toNativeUtf8();
    final ret = _containsKey(_handle, keyPtr);
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  int get count {
    return _count(_handle);
  }

  /// Get the file size. See also [actualSize].
  int get totalSize {
    return _totalSize(_handle);
  }

  /// Get the actual used size. See also [totalSize].
  int get actualSize {
    return _actualSize(_handle);
  }

  void removeValue(String key) {
    final keyPtr = key.toNativeUtf8();
    _removeValueForKey(_handle, keyPtr);
    calloc.free(keyPtr);
  }

  /// See also [trim()].
  void removeValues(List<String> keys) {
    if (keys.isEmpty) {
      return;
    }
    final Pointer<Pointer<Utf8>> keyArray = calloc<Pointer<Utf8>>(keys.length);
    final Pointer<Uint32> sizeArray = malloc<Uint32>(keys.length);
    for (int index = 0; index < keys.length; index++) {
      final key = keys[index];
      final bytes = MMBuffer.fromList(const Utf8Encoder().convert(key))!;
      sizeArray[index] = bytes.length;
      keyArray[index] = bytes.pointer!.cast();
    }

    _removeValuesForKeys(_handle, keyArray, sizeArray, keys.length);

    for (int index = 0; index < keys.length; index++) {
      calloc.free(keyArray[index]);
    }
    calloc.free(keyArray);
    calloc.free(sizeArray);
  }

  void clearAll() {
    _clearAll(_handle);
  }

  /// Synchronize memory to file.
  /// You don't need to call this, really, I mean it.
  /// Unless you worry about running out of battery.
  /// * Pass `true` to perform synchronous write.
  /// * Pass `false` to perform asynchronous write, return immediately.
  void sync(bool sync) {
    _mmkvSync(_handle, _bool2Int(sync));
  }

  /// Clear all caches (on memory warning).
  void clearMemoryCache() {
    _clearMemoryCache(_handle);
  }

  /// Get memory page size.
  static int get pageSize {
    return _pageSize();
  }

  static String get version {
    return _pointer2String(_version())!;
  }

  /// Trim the file size to minimal.
  ///
  /// * MMKV's size won't reduce after deleting key-values.
  /// * Call this method after lots of deleting if you care about disk usage.
  /// * Note that [clearAll()] has the similar effect.
  void trim() {
    _trim(_handle);
  }

  /// Close the instance when it's no longer needed in the near future.
  /// Any subsequent call to the instance is **undefined behavior**.
  void close() {
    _mmkvClose(_handle);
  }

  /// backup one MMKV instance to [dstDir]
  ///
  /// * [rootDir] the customize root path of the MMKV, if null then backup from the root dir of MMKV
  static bool backupOneToDirectory(String mmapID, String dstDir,
      {String? rootDir}) {
    final mmapIDPtr = mmapID.toNativeUtf8();
    final dstDirPtr = dstDir.toNativeUtf8();
    final rootDirPtr = _string2Pointer(rootDir);

    final ret = _backupOne(mmapIDPtr, dstDirPtr, rootDirPtr);

    calloc.free(mmapIDPtr);
    calloc.free(dstDirPtr);
    calloc.free(rootDirPtr);

    return _int2Bool(ret);
  }

  /// restore one MMKV instance from [srcDir]
  ///
  /// * [rootDir] the customize root path of the MMKV, if null then restore to the root dir of MMKV
  static bool restoreOneMMKVFromDirectory(String mmapID, String srcDir,
      {String? rootDir}) {
    final mmapIDPtr = mmapID.toNativeUtf8();
    final srcDirPtr = srcDir.toNativeUtf8();
    final rootDirPtr = _string2Pointer(rootDir);

    final ret = _restoreOne(mmapIDPtr, srcDirPtr, rootDirPtr);

    calloc.free(mmapIDPtr);
    calloc.free(srcDirPtr);
    calloc.free(rootDirPtr);

    return _int2Bool(ret);
  }

  /// backup all MMKV instance to [dstDir]
  static int backupAllToDirectory(String dstDir) {
    final dstDirPtr = dstDir.toNativeUtf8();

    final ret = _backupAll(dstDirPtr);

    calloc.free(dstDirPtr);

    return ret;
  }

  /// restore all MMKV instance from [srcDir]
  static int restoreAllFromDirectory(String srcDir) {
    final srcDirPtr = srcDir.toNativeUtf8();

    final ret = _restoreAll(srcDirPtr);

    calloc.free(srcDirPtr);

    return ret;
  }
}

/* Looks like Dart:ffi's async callback not working perfectly
 * We don't support them for the moment.
 * https://github.com/dart-lang/sdk/issues/37022
class MMKV {
  ....
  // callbacks
  static void registerLogCallback(LogCallback callback) {
    _logCallback = callback;
    _setWantsLogRedirect(Pointer.fromFunction<_LogCallbackWrap>(_logRedirect));
  }

  static void unRegisterLogCallback() {
    _setWantsLogRedirect(nullptr);
    _logCallback = null;
  }
}

typedef LogCallback = void Function(MMKVLogLevel level, String file, int line, String funcname, String message);
typedef _LogCallbackWrap = Void Function(Uint32, Pointer<Utf8>, Int32, Pointer<Utf8>, Pointer<Utf8>);
typedef _LogCallbackRegisterWrap = Void Function(Pointer<NativeFunction<_LogCallbackWrap>>);
typedef _LogCallbackRegister = void Function(Pointer<NativeFunction<_LogCallbackWrap>>);
LogCallback _logCallback;

void _logRedirect(int logLevel, Pointer<Utf8> file, int line, Pointer<Utf8> funcname, Pointer<Utf8> message) {
  if (_logCallback == null) {
    return;
  }

  MMKVLogLevel level;
  switch (logLevel) {
    case 0:
      level = MMKVLogLevel.Debug;
      break;
    case 1:
      level = MMKVLogLevel.Info;
      break;
    case 2:
      level = MMKVLogLevel.Warning;
      break;
    case 3:
      level = MMKVLogLevel.Error;
      break;
    case 4:
    default:
      level = MMKVLogLevel.None;
      break;
  }

  _logCallback(level, _pointer2String(file), line, _pointer2String(funcname), _pointer2String(message));

  if (!Platform.isIOS) {
    free(message);
  }
}

final _LogCallbackRegister _setWantsLogRedirect =
nativeLib.lookup<NativeFunction<_LogCallbackRegisterWrap>>('setWantsLogRedirect')
    .asFunction();
*/

int _bool2Int(bool value) {
  return value ? 1 : 0;
}

bool _int2Bool(int value) {
  return (value != 0) ? true : false;
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

String? _buffer2String(Pointer<Uint8>? ptr, int length) {
  if (ptr != null && ptr != nullptr) {
    final listView = ptr.asTypedList(length);
    return const Utf8Decoder().convert(listView);
  }
  return null;
}

String _nativeFuncName(String name) {
  if (!Platform.isIOS) {
    return name;
  }
  return "mmkv_$name";
}

final DynamicLibrary _nativeLib = Platform.isAndroid
    ? DynamicLibrary.open("libmmkv.so")
    : DynamicLibrary.process();

final void Function(Pointer<Utf8> rootDir, Pointer<Utf8> cacheDir, int sdkInt,
        int logLevel)? _mmkvInitialize =
    Platform.isIOS
        ? null
        : _nativeLib
            .lookup<
                NativeFunction<
                    Void Function(Pointer<Utf8>, Pointer<Utf8>, Int32,
                        Int32)>>("mmkvInitialize_v1")
            .asFunction();

final Pointer<Void> Function(Pointer<Utf8> mmapID, int, Pointer<Utf8> cryptKey,
        Pointer<Utf8> rootDir) _getMMKVWithID =
    _nativeLib
        .lookup<
            NativeFunction<
                Pointer<Void> Function(Pointer<Utf8>, Uint32, Pointer<Utf8>,
                    Pointer<Utf8>)>>("getMMKVWithID")
        .asFunction();

final Pointer<Void> Function(int, Pointer<Utf8> cryptKey) _getDefaultMMKV =
    _nativeLib
        .lookup<NativeFunction<Pointer<Void> Function(Uint32, Pointer<Utf8>)>>(
            "getDefaultMMKV")
        .asFunction();

final Pointer<Utf8> Function(Pointer<Void>) _mmapID = _nativeLib
    .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
        _nativeFuncName("mmapID"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeBool = _nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>(
        _nativeFuncName("encodeBool"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeBool = _nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>(
        _nativeFuncName("decodeBool"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeInt32 = _nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int32)>>(
        _nativeFuncName("encodeInt32"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeInt32 = _nativeLib
    .lookup<
        NativeFunction<
            Int32 Function(Pointer<Void>, Pointer<Utf8>,
                Int32)>>(_nativeFuncName("decodeInt32"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeInt64 = _nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int64)>>(
        _nativeFuncName("encodeInt64"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeInt64 = _nativeLib
    .lookup<
        NativeFunction<
            Int64 Function(Pointer<Void>, Pointer<Utf8>,
                Int64)>>(_nativeFuncName("decodeInt64"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, double) _encodeDouble =
    _nativeLib
        .lookup<
            NativeFunction<
                Int8 Function(Pointer<Void>, Pointer<Utf8>,
                    Double)>>(_nativeFuncName("encodeDouble"))
        .asFunction();

final double Function(Pointer<Void>, Pointer<Utf8>, double) _decodeDouble =
    _nativeLib
        .lookup<
            NativeFunction<
                Double Function(Pointer<Void>, Pointer<Utf8>,
                    Double)>>(_nativeFuncName("decodeDouble"))
        .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int)
    _encodeBytes = _nativeLib
        .lookup<
            NativeFunction<
                Int8 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>,
                    Uint64)>>(_nativeFuncName("encodeBytes"))
        .asFunction();

final Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint64>)
    _decodeBytes = _nativeLib
        .lookup<
            NativeFunction<
                Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>,
                    Pointer<Uint64>)>>(_nativeFuncName("decodeBytes"))
        .asFunction();

final int Function(Pointer<Void>, Pointer<Uint8>, int) _reKey = _nativeLib
    .lookup<
        NativeFunction<
            Int8 Function(Pointer<Void>, Pointer<Uint8>,
                Uint64)>>(_nativeFuncName("reKey"))
    .asFunction();

final Pointer<Uint8> Function(Pointer<Void>, Pointer<Uint64>) _cryptKey =
    _nativeLib
        .lookup<
            NativeFunction<
                Pointer<Uint8> Function(Pointer<Void>,
                    Pointer<Uint64>)>>(_nativeFuncName("cryptKey"))
        .asFunction();

final void Function(Pointer<Void>, Pointer<Uint8>, int) _checkReSetCryptKey =
    _nativeLib
        .lookup<
            NativeFunction<
                Void Function(Pointer<Void>, Pointer<Uint8>,
                    Uint64)>>(_nativeFuncName("checkReSetCryptKey"))
        .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _valueSize = _nativeLib
    .lookup<
        NativeFunction<
            Uint32 Function(Pointer<Void>, Pointer<Utf8>,
                Int8)>>(_nativeFuncName("valueSize"))
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>, int)
    _writeValueToNB = _nativeLib
        .lookup<
            NativeFunction<
                Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>,
                    Uint32)>>(_nativeFuncName("writeValueToNB"))
        .asFunction();

final int Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>,
        Pointer<Pointer<Uint32>>) _allKeys =
    _nativeLib
        .lookup<
            NativeFunction<
                Uint64 Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>,
                    Pointer<Pointer<Uint32>>)>>(_nativeFuncName("allKeys"))
        .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>) _containsKey = _nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>)>>(
        _nativeFuncName("containsKey"))
    .asFunction();

final int Function(Pointer<Void>) _count = _nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>(
        _nativeFuncName("count"))
    .asFunction();

final int Function(Pointer<Void>) _totalSize = _nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>(
        _nativeFuncName("totalSize"))
    .asFunction();

final int Function(Pointer<Void>) _actualSize = _nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>(
        _nativeFuncName("actualSize"))
    .asFunction();

final void Function(Pointer<Void>, Pointer<Utf8>) _removeValueForKey =
    _nativeLib
        .lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Utf8>)>>(
            _nativeFuncName("removeValueForKey"))
        .asFunction();

final void Function(Pointer<Void>, Pointer<Pointer<Utf8>>, Pointer<Uint32>, int)
    _removeValuesForKeys = _nativeLib
        .lookup<
            NativeFunction<
                Void Function(
                    Pointer<Void>,
                    Pointer<Pointer<Utf8>>,
                    Pointer<Uint32>,
                    Uint64)>>(_nativeFuncName("removeValuesForKeys"))
        .asFunction();

final void Function(Pointer<Void>) _clearAll = _nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>(
        _nativeFuncName("clearAll"))
    .asFunction();

final void Function(Pointer<Void>, int) _mmkvSync = _nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>, Int8)>>("mmkvSync")
    .asFunction();

final void Function(Pointer<Void>) _clearMemoryCache = _nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>(
        _nativeFuncName("clearMemoryCache"))
    .asFunction();

final int Function() _pageSize = _nativeLib
    .lookup<NativeFunction<Int32 Function()>>(_nativeFuncName("pageSize"))
    .asFunction();

final Pointer<Utf8> Function() _version = _nativeLib
    .lookup<NativeFunction<Pointer<Utf8> Function()>>(
        _nativeFuncName("version"))
    .asFunction();

final void Function(Pointer<Void>) _trim = _nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>(
        _nativeFuncName("trim"))
    .asFunction();

final void Function(Pointer<Void>) _mmkvClose = _nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>("mmkvClose")
    .asFunction();

final void Function(Pointer<Void>, Pointer<Void>, int) _memcpy = _nativeLib
    .lookup<
        NativeFunction<
            Void Function(Pointer<Void>, Pointer<Void>, Uint64)>>("mmkvMemcpy")
    .asFunction();

final int Function(
        Pointer<Utf8> mmapID, Pointer<Utf8> dstDir, Pointer<Utf8> rootPath)
    _backupOne = _nativeLib
        .lookup<
            NativeFunction<
                Int8 Function(Pointer<Utf8>, Pointer<Utf8>,
                    Pointer<Utf8>)>>(_nativeFuncName("backupOne"))
        .asFunction();

final int Function(
        Pointer<Utf8> mmapID, Pointer<Utf8> srcDir, Pointer<Utf8> rootPath)
    _restoreOne = _nativeLib
        .lookup<
            NativeFunction<
                Int8 Function(Pointer<Utf8>, Pointer<Utf8>,
                    Pointer<Utf8>)>>(_nativeFuncName("restoreOne"))
        .asFunction();

final int Function(Pointer<Utf8> dstDir) _backupAll = _nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Utf8>)>>(
        _nativeFuncName("backupAll"))
    .asFunction();

final int Function(Pointer<Utf8> srcDir) _restoreAll = _nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Utf8>)>>(
        _nativeFuncName("restoreAll"))
    .asFunction();
