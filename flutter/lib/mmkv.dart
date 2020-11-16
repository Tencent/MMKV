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

import 'dart:async';
import 'dart:ffi'; // For FFI
import 'dart:io'; // For Platform.isX
import 'dart:typed_data';
import 'package:ffi/ffi.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:convert';

import 'package:flutter/services.dart';

enum MMKVLogLevel {
  Debug,
  Info,
  Warning,
  Error,
  None
}

enum MMKVMode {
  INVALID_MODE,
  SINGLE_PROCESS_MODE,
  MULTI_PROCESS_MODE,
}

// a native memory buffer, must call destroy() after no longer use
class MMBuffer {
  int _length;
  int get length => _length;

  Pointer<Uint8> _ptr;
  Pointer<Uint8> get pointer => _ptr;

  MMBuffer(int size) {
    _length = size;
    if (size > 0) {
      _ptr = allocate<Uint8>(count: size);
    }
  }

  // copy all data from list
  static MMBuffer fromList(List<int> list) {
    var buffer = MMBuffer(list.length);
    buffer.asList().setAll(0, list);
    return buffer;
  }

  static MMBuffer _fromPointer(Pointer<Uint8> ptr, int length) {
    var buffer = MMBuffer(0);
    buffer._length = length;
    buffer._ptr = ptr;
    return buffer;
  }

  static MMBuffer _copyFromPointer(Pointer<Uint8> ptr, int length) {
    var buffer = MMBuffer(length);
    buffer._length = length;
    _memcpy(buffer.pointer.cast(), ptr.cast(), length);
    return buffer;
  }

  // must call destroy after no longer use
  void destroy() {
    if (_ptr != null && _ptr != nullptr) {
      free(_ptr);
    }
    _ptr = null;
  }

  // get a list view of the underlying data
  Uint8List asList() {
    if (_ptr != null && _ptr != nullptr) {
      return _ptr.asTypedList(_length);
    }
    return null;
  }

  // copy the underlying data as a list
  // and destroy() self
  Uint8List takeList() {
    if (_ptr != null && _ptr != nullptr) {
      var list = Uint8List.fromList(asList());
      destroy();
      return list;
    }
    return null;
  }
}

class MMKV {
  Pointer<Void> _handle;

  static const MethodChannel _channel = const MethodChannel('mmkv');

  static Future<String> initialize([String rootDir, String groupDir, MMKVLogLevel logLevel = MMKVLogLevel.Info]) async {
    WidgetsFlutterBinding.ensureInitialized();

    if (rootDir == null) {
      final path = await getApplicationDocumentsDirectory();
      rootDir = path.path + '/mmkv';
    }

    if (Platform.isIOS) {
      final Map<String, dynamic> params = {
        'rootDir' : rootDir,
        'logLevel' : logLevel.index,
      };
      if (groupDir != null) {
        params['groupDir'] = groupDir;
      }
      final ret = await _channel.invokeMethod('initializeMMKV', params);
      return ret;
    } else {
      final rootDirPtr = Utf8.toUtf8(rootDir);
      _mmkvInitialize(rootDirPtr, logLevel.index);
      free(rootDirPtr);
      return rootDir;
    }
  }

  MMKV(String mmapID, [MMKVMode mode = MMKVMode.SINGLE_PROCESS_MODE, String cryptKey, String rootDir]) {
    if (mmapID != null) {
      var mmapIDPtr = _string2Pointer(mmapID);
      var cryptKeyPtr = _string2Pointer(cryptKey);
      var rootDirPtr = _string2Pointer(rootDir);

      _handle = _getMMKVWithID(mmapIDPtr, mode.index, cryptKeyPtr, rootDirPtr);

      free(mmapIDPtr);
      free(cryptKeyPtr);
      free(rootDirPtr);
    }
  }

  static MMKV defaultMMKV([String cryptKey]) {
    var mmkv = MMKV(null);
    var cryptKeyPtr = _string2Pointer(cryptKey);
    mmkv._handle = _getDefaultMMKV(cryptKeyPtr);
    free(cryptKeyPtr);
    return mmkv;
  }

  String mmapID() {
    return _pointer2String(_mmapID(_handle));
  }

  bool encodeBool(String key, bool value) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _encodeBool(_handle, keyPtr, _bool2Int(value));
    free(keyPtr);
    return _int2Bool(ret);
  }

  bool decodeBool(String key, [bool defaultValue = false]) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _decodeBool(_handle, keyPtr, _bool2Int(defaultValue));
    free(keyPtr);
    return _int2Bool(ret);
  }

  bool encodeInt32(String key, int value) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _encodeInt32(_handle, keyPtr, value);
    free(keyPtr);
    return _int2Bool(ret);
  }

  int decodeInt32(String key, [int defaultValue = 0]) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _decodeInt32(_handle, keyPtr, defaultValue);
    free(keyPtr);
    return ret;
  }

  bool encodeInt(String key, int value) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _encodeInt64(_handle, keyPtr, value);
    free(keyPtr);
    return _int2Bool(ret);
  }

  int decodeInt(String key, [int defaultValue = 0]) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _decodeInt64(_handle, keyPtr, defaultValue);
    free(keyPtr);
    return ret;
  }

  bool encodeDouble(String key, double value) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _encodeDouble(_handle, keyPtr, value);
    free(keyPtr);
    return _int2Bool(ret);
  }

  double decodeDouble(String key, [double defaultValue = 0]) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _decodeDouble(_handle, keyPtr, defaultValue);
    free(keyPtr);
    return ret;
  }

  bool encodeString(String key, String value) {
    var keyPtr = Utf8.toUtf8(key);
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(value));

    var ret = _encodeBytes(_handle, keyPtr, bytes.pointer, bytes.length);

    free(keyPtr);
    bytes.destroy();
    return _int2Bool(ret);
  }

  String decodeString(String key) {
    var keyPtr = Utf8.toUtf8(key);
    Pointer<Uint64> lengthPtr = allocate();

    var ret = _decodeBytes(_handle, keyPtr, lengthPtr);
    free(keyPtr);

    if (ret != null && ret != nullptr) {
      var length = lengthPtr.value;
      free(lengthPtr);
      var result = _buffer2String(ret, length);
      if (!Platform.isIOS) {
        free(ret);
      }
      return result;
    }
    free(lengthPtr);
    return null;
  }

  bool encodeBytes(String key, MMBuffer value) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _encodeBytes(_handle, keyPtr, value.pointer, value.length);
    free(keyPtr);
    return _int2Bool(ret);
  }

  MMBuffer decodeBytes(String key) {
    var keyPtr = Utf8.toUtf8(key);
    Pointer<Uint64> lengthPtr = allocate();

    var ret = _decodeBytes(_handle, keyPtr, lengthPtr);
    free(keyPtr);

    if (ret != null && ret != nullptr) {
      var length = lengthPtr.value;
      free(lengthPtr);
      if (Platform.isIOS) {
        return MMBuffer._copyFromPointer(ret, length);
      } else {
        return MMBuffer._fromPointer(ret, length);
      }
    }
    free(lengthPtr);
    return null;
  }

  bool reKey(String cryptKey) {
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(cryptKey));
    var ret = _reKey(_handle, bytes.pointer, bytes.length);
    bytes.destroy();
    return _int2Bool(ret);
  }

  String cryptKey() {
    Pointer<Uint64> lengthPtr = allocate();
    var ret = _cryptKey(_handle, lengthPtr);
    if (ret != null && ret != nullptr) {
      var length = lengthPtr.value;
      free(lengthPtr);
      var result = _buffer2String(ret, length);
      free(ret);
      return result;
    }
    return null;
  }

  void checkReSetCryptKey(String cryptKey) {
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(cryptKey));
    _checkReSetCryptKey(_handle, bytes.pointer, bytes.length);
    bytes.destroy();
  }

  int valueSize(String key, bool actualSize) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _valueSize(_handle, keyPtr, _bool2Int(actualSize));
    free(keyPtr);
    return ret;
  }

  int writeValueToNativeBuffer(String key, MMBuffer buffer) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _writeValueToNB(_handle, keyPtr, buffer.pointer.cast(), buffer.length);
    free(keyPtr);
    return ret;
  }

  List<String> allKeys() {
    Pointer<Pointer<Pointer<Utf8>>> keyArrayPtr = allocate();
    Pointer<Pointer<Uint32>> sizeArrayPtr = allocate();
    List<String> keys;

    final count = _allKeys(_handle, keyArrayPtr, sizeArrayPtr);
    if (count > 0) {
      keys = [];
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
          free(keyPtr);
        }
      }
      free(keyArray);
      free(sizeArray);
    }

    free(sizeArrayPtr);
    free(keyArrayPtr);

    return keys;
  }

  bool containsKey(String key) {
    var keyPtr = Utf8.toUtf8(key);
    var ret = _containsKey(_handle, keyPtr);
    free(keyPtr);
    return _int2Bool(ret);
  }

  int count() {
    return _count(_handle);
  }

  int totalSize() {
    return _totalSize(_handle);
  }

  int actualSize() {
    return _actualSize(_handle);
  }

  void removeValue(String key) {
    var keyPtr = Utf8.toUtf8(key);
    _removeValueForKey(_handle, keyPtr);
    free(keyPtr);
  }

  void removeValues(List<String> keys) {
    if (keys.isEmpty) {
      return;
    }
    Pointer<Pointer<Utf8>> keyArray = allocate(count: keys.length);
    Pointer<Uint32> sizeArray = allocate(count: keys.length);
    for (int index = 0; index < keys.length; index++) {
      final key = keys[index];
      var bytes = MMBuffer.fromList(Utf8Encoder().convert(key));
      sizeArray[index] = bytes.length;
      keyArray[index] = bytes.pointer.cast();
    }

    _removeValuesForKeys(_handle, keyArray, sizeArray, keys.length);

    for (int index = 0; index < keys.length; index++) {
      free(keyArray[index]);
    }
    free(keyArray);
    free(sizeArray);
  }

  void clearAll() {
    _clearAll(_handle);
  }

  void sync(bool sync) {
    _mmkvSync(_handle, _bool2Int(sync));
  }

  void clearMemoryCache() {
    _clearMemoryCache(_handle);
  }

  static int pageSize() {
    return _pageSize();
  }

  void trim() {
    _trim(_handle);
  }

  void close() {
    _mmkvClose(_handle);
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

Pointer<Utf8> _string2Pointer(String str) {
  if (str != null) {
    return Utf8.toUtf8(str);
  }
  return nullptr;
}

String _pointer2String(Pointer<Utf8> ptr) {
  if (ptr != null && ptr != nullptr) {
    return Utf8.fromUtf8(ptr);
  }
  return null;
}

String _buffer2String(Pointer<Uint8> ptr, int length) {
  if (ptr != null && ptr != nullptr) {
    var listView = ptr.asTypedList(length);
    return Utf8Decoder().convert(listView);
  }
  return null;
}

final DynamicLibrary nativeLib = Platform.isAndroid
    ? DynamicLibrary.open("libmmkv.so")
    : DynamicLibrary.process();

final void Function(Pointer<Utf8> rootDir, int logLevel) _mmkvInitialize =
  Platform.isIOS ? null : nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Utf8>, Int32)>>("mmkvInitialize")
    .asFunction();

final Pointer<Void> Function(Pointer<Utf8> mmapID, int, Pointer<Utf8> cryptKey, Pointer<Utf8> rootDir) _getMMKVWithID =
nativeLib
    .lookup<NativeFunction<Pointer<Void> Function(Pointer<Utf8>, Uint32, Pointer<Utf8>, Pointer<Utf8>)>>("getMMKVWithID")
    .asFunction();

final Pointer<Void> Function(Pointer<Utf8> cryptKey) _getDefaultMMKV =
nativeLib
    .lookup<NativeFunction<Pointer<Void> Function(Pointer<Utf8>)>>("getDefaultMMKV")
    .asFunction();

final Pointer<Utf8> Function(Pointer<Void>) _mmapID =
nativeLib
    .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>("mmapID")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeBool =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>("encodeBool")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeBool =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>("decodeBool")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeInt32 =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int32)>>("encodeInt32")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeInt32 =
nativeLib
    .lookup<NativeFunction<Int32 Function(Pointer<Void>, Pointer<Utf8>, Int32)>>("decodeInt32")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeInt64 =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Int64)>>("encodeInt64")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeInt64 =
nativeLib
    .lookup<NativeFunction<Int64 Function(Pointer<Void>, Pointer<Utf8>, Int64)>>("decodeInt64")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, double) _encodeDouble =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Double)>>("encodeDouble")
    .asFunction();

final double Function(Pointer<Void>, Pointer<Utf8>, double) _decodeDouble =
nativeLib
    .lookup<NativeFunction<Double Function(Pointer<Void>, Pointer<Utf8>, Double)>>("decodeDouble")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int) _encodeBytes =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, Uint64)>>("encodeBytes")
    .asFunction();

final Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint64>) _decodeBytes =
nativeLib
    .lookup<NativeFunction<Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint64>)>>("decodeBytes")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Uint8>, int) _reKey =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Uint8>, Uint64)>>("reKey")
    .asFunction();

final Pointer<Uint8> Function(Pointer<Void>, Pointer<Uint64>) _cryptKey =
nativeLib
    .lookup<NativeFunction<Pointer<Uint8> Function(Pointer<Void>, Pointer<Uint64>)>>("cryptKey")
    .asFunction();

final void Function(Pointer<Void>, Pointer<Uint8>, int) _checkReSetCryptKey =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Uint8>, Uint64)>>("checkReSetCryptKey")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _valueSize =
nativeLib
    .lookup<NativeFunction<Uint32 Function(Pointer<Void>, Pointer<Utf8>, Int8)>>("valueSize")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>, int) _writeValueToNB =
nativeLib
    .lookup<NativeFunction<Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>, Uint32)>>("writeValueToNB")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>, Pointer<Pointer<Uint32>>) _allKeys =
nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>, Pointer<Pointer<Uint32>>)>>("allKeys")
    .asFunction();

final int Function(Pointer<Void>, Pointer<Utf8>) _containsKey =
nativeLib
    .lookup<NativeFunction<Int8 Function(Pointer<Void>, Pointer<Utf8>)>>("containsKey")
    .asFunction();

final int Function(Pointer<Void>) _count =
nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>("count")
    .asFunction();

final int Function(Pointer<Void>) _totalSize =
nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>("totalSize")
    .asFunction();

final int Function(Pointer<Void>) _actualSize =
nativeLib
    .lookup<NativeFunction<Uint64 Function(Pointer<Void>)>>("actualSize")
    .asFunction();

final void Function(Pointer<Void>, Pointer<Utf8>) _removeValueForKey =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Utf8>)>>("removeValueForKey")
    .asFunction();

final void Function(Pointer<Void>, Pointer<Pointer<Utf8>>, Pointer<Uint32>, int) _removeValuesForKeys =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Pointer<Utf8>>, Pointer<Uint32>, Uint64)>>("removeValuesForKeys")
    .asFunction();

final void Function(Pointer<Void>) _clearAll =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>("clearAll")
    .asFunction();

final void Function(Pointer<Void>, int) _mmkvSync =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>, Int8)>>("mmkvSync")
    .asFunction();

final void Function(Pointer<Void>) _clearMemoryCache =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>("clearMemoryCache")
    .asFunction();

final int Function() _pageSize =
nativeLib
    .lookup<NativeFunction<Int32 Function()>>("pageSize")
    .asFunction();

final void Function(Pointer<Void>) _trim =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>("trim")
    .asFunction();

final void Function(Pointer<Void>) _mmkvClose =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>)>>("mmkvClose")
    .asFunction();

final void Function(Pointer<Void>, Pointer<Void>, int) _memcpy =
nativeLib
    .lookup<NativeFunction<Void Function(Pointer<Void>, Pointer<Void>, Uint64)>>("mmkvMemcpy")
    .asFunction();
