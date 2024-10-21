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
import "package:mmkv_platform_interface/mmkv_platform_interface.dart";

export "package:mmkv_platform_interface/mmkv_platform_interface.dart" show MMKVHandler, MMKVLogLevel, MMKVRecoverStrategic;

/// Process mode for MMKV, default to [SINGLE_PROCESS_MODE].
enum MMKVMode {
  INVALID_MODE,
  SINGLE_PROCESS_MODE,
  MULTI_PROCESS_MODE,
}
final int _READ_ONLY_MODE = 1 << 5;

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
  static Future<String> initialize({String? rootDir, String? groupDir, MMKVLogLevel logLevel = MMKVLogLevel.Info, MMKVHandler? handler}) async {
    WidgetsFlutterBinding.ensureInitialized();

    if (rootDir == null) {
      final path = await _mmkvPlatform.getApplicationDocumentsPath();
      rootDir = "${path}/mmkv";
    }
    _rootDir = rootDir;

    _mmkvPlatform.theHandler = handler;
    final logHandler = (handler != null && handler.wantLogRedirect()) ? Pointer.fromFunction<LogCallbackWrap>(_logRedirect) : nullptr;

    final result = await _mmkvPlatform.initialize(rootDir, groupDir: groupDir, logLevel: logLevel.index, logHandler: logHandler);
    if (handler != null) {
      const ExceptionalReturn = -1;
      final errorHandler = Pointer.fromFunction<ErrorCallbackWrap>(_errorHandler, ExceptionalReturn);
      _registerErrorHandler(errorHandler);

      if (handler.wantContentChangeNotification()) {
        final contentHandler = Pointer.fromFunction<ContentCallbackWrap>(_contentChangeHandler);
        _registerContentHandler(contentHandler);
      }
    }
    return result;
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
    if (mmkv._handle == nullptr) {
      throw StateError("Invalid state, forget initialize MMKV first?");
    }
    calloc.free(cryptKeyPtr);
    return mmkv;
  }

  /// Get an MMKV instance with an unique ID [mmapID].
  ///
  /// * If you want a per-user mmkv, you could merge user-id within [mmapID].
  /// * You can get a multi-process MMKV instance by passing [MMKVMode.MULTI_PROCESS_MODE].
  /// * You can encrypt with [cryptKey], which limits to 16 bytes at most.
  /// * You can customize the [rootDir] of the file.
  MMKV(String mmapID, {MMKVMode mode = MMKVMode.SINGLE_PROCESS_MODE, String? cryptKey, String? rootDir, int expectedCapacity = 0
    , bool readOnly = false}) {
    if (mmapID.isNotEmpty) {
      final mmapIDPtr = _string2Pointer(mmapID);
      final cryptKeyPtr = _string2Pointer(cryptKey);
      final rootDirPtr = _string2Pointer(rootDir);

      final realMode = readOnly ? (mode.index | _READ_ONLY_MODE) : mode.index;
      _handle = _getMMKVWithID(mmapIDPtr, realMode, cryptKeyPtr, rootDirPtr, expectedCapacity);
      if (_handle == nullptr) {
        throw StateError("Invalid state, forget initialize MMKV first?");
      }

      calloc.free(mmapIDPtr);
      calloc.free(cryptKeyPtr);
      calloc.free(rootDirPtr);
    }
  }

  String get mmapID {
    return _pointer2String(_mmapID(_handle))!;
  }

  static const int ExpireNever = 0;
  static const int ExpireInMinute = 60;
  static const int ExpireInHour = 60 * 60;
  static const int ExpireInDay = 24 * 60 * 60;
  static const int ExpireInMonth = 30 * 24 * 60 * 60;
  static const int ExpireInYear = 365 * 30 * 24 * 60 * 60;

  /// [expireDurationInSecond] override the default duration setting from [enableAutoKeyExpire()].
  /// * Passing [MMKV.ExpireNever] (aka 0) will never expire.
  bool encodeBool(String key, bool value, [int? expireDurationInSecond]) {
    final keyPtr = key.toNativeUtf8();
    final ret = (expireDurationInSecond == null)
        ? _encodeBool(_handle, keyPtr, _bool2Int(value))
        : _encodeBoolV2(_handle, keyPtr, _bool2Int(value), expireDurationInSecond);
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
  /// [expireDurationInSecond] override the default duration setting from [enableAutoKeyExpire()].
  /// * Passing [MMKV.ExpireNever] (aka 0) will never expire.
  bool encodeInt32(String key, int value, [int? expireDurationInSecond]) {
    final keyPtr = key.toNativeUtf8();
    final ret =
        (expireDurationInSecond == null) ? _encodeInt32(_handle, keyPtr, value) : _encodeInt32V2(_handle, keyPtr, value, expireDurationInSecond);
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

  /// [expireDurationInSecond] override the default duration setting from [enableAutoKeyExpire()].
  /// * Passing [MMKV.ExpireNever] (aka 0) will never expire.
  bool encodeInt(String key, int value, [int? expireDurationInSecond]) {
    final keyPtr = key.toNativeUtf8();
    final ret =
        (expireDurationInSecond == null) ? _encodeInt64(_handle, keyPtr, value) : _encodeInt64V2(_handle, keyPtr, value, expireDurationInSecond);
    calloc.free(keyPtr);
    return _int2Bool(ret);
  }

  int decodeInt(String key, {int defaultValue = 0}) {
    final keyPtr = key.toNativeUtf8();
    final ret = _decodeInt64(_handle, keyPtr, defaultValue);
    calloc.free(keyPtr);
    return ret;
  }

  /// [expireDurationInSecond] override the default duration setting from [enableAutoKeyExpire()].
  /// * Passing [MMKV.ExpireNever] (aka 0) will never expire.
  bool encodeDouble(String key, double value, [int? expireDurationInSecond]) {
    final keyPtr = key.toNativeUtf8();
    final ret =
        (expireDurationInSecond == null) ? _encodeDouble(_handle, keyPtr, value) : _encodeDoubleV2(_handle, keyPtr, value, expireDurationInSecond);
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
  /// [expireDurationInSecond] override the default duration setting from [enableAutoKeyExpire()].
  /// * Passing [MMKV.ExpireNever] (aka 0) will never expire.
  bool encodeString(String key, String? value, [int? expireDurationInSecond]) {
    if (value == null) {
      removeValue(key);
      return true;
    }

    final keyPtr = key.toNativeUtf8();
    final bytes = MMBuffer.fromList(const Utf8Encoder().convert(value))!;

    final ret = (expireDurationInSecond == null)
        ? _encodeBytes(_handle, keyPtr, bytes.pointer!, bytes.length)
        : _encodeBytesV2(_handle, keyPtr, bytes.pointer!, bytes.length, expireDurationInSecond);

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
  /// [expireDurationInSecond] override the default duration setting from [enableAutoKeyExpire()].
  /// * Passing [MMKV.ExpireNever] (aka 0) will never expire.
  bool encodeBytes(String key, MMBuffer? value, [int? expireDurationInSecond]) {
    if (value == null) {
      removeValue(key);
      return true;
    }

    final keyPtr = key.toNativeUtf8();
    final ret = (expireDurationInSecond == null)
        ? _encodeBytes(_handle, keyPtr, value.pointer!, value.length)
        : _encodeBytesV2(_handle, keyPtr, value.pointer!, value.length, expireDurationInSecond);
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

  /// Write the value to a pre-allocated native buffer.
  ///
  /// * Return size written into buffer.
  /// * Return -1 on any error, such as [buffer] not large enough.
  int writeValueToNativeBuffer(String key, MMBuffer buffer) {
    final keyPtr = key.toNativeUtf8();
    final ret = _writeValueToNB(_handle, keyPtr, buffer.pointer!.cast(), buffer.length);
    calloc.free(keyPtr);
    return ret;
  }

  /// Get all the keys (_unsorted_).
  List<String> get allKeys {
    return _allKeysImp(false);
  }

  /// Get all non-expired keys (_unsorted_). Note that this call has costs.
  List<String> get allNonExpiredKeys {
    return _allKeysImp(true);
  }

  List<String> _allKeysImp(bool filterExpire) {
    final keyArrayPtr = calloc<Pointer<Pointer<Utf8>>>();
    final sizeArrayPtr = calloc<Pointer<Uint32>>();
    final List<String> keys = [];

    final count = _allKeys(_handle, keyArrayPtr, sizeArrayPtr, _bool2Int(filterExpire));
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
    return _count(_handle, _bool2Int(false));
  }

  /// Get non-expired keys. Note that this call has costs.
  int get countNonExpiredKeys {
    return _count(_handle, _bool2Int(true));
  }

  /// Get the file size. See also [actualSize].
  int get totalSize {
    return _totalSize(_handle);
  }

  /// Get the actual used size. See also [totalSize].
  int get actualSize {
    return _actualSize(_handle);
  }

  bool get isMultiProcess {
    return _isMultiProcess(_handle);
  }

  bool get isReadOnly {
    return _isReadOnly(_handle);
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

  void clearAll({bool keepSpace = false}) {
    _clearAll(_handle, _bool2Int(keepSpace));
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
  static bool backupOneToDirectory(String mmapID, String dstDir, {String? rootDir}) {
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
  static bool restoreOneMMKVFromDirectory(String mmapID, String srcDir, {String? rootDir}) {
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

  /// Enable auto key expiration. This is a upgrade operation, the file format will change.
  /// And the file won't be accessed correctly by older version (v1.2.17) of MMKV.
  /// [expireDurationInSecond] the expire duration for all keys, [MMKV.ExpireNever] (0) means no default duration (aka each key will have it's own expire date)
  bool enableAutoKeyExpire(int expiredInSeconds) {
    return _enableAutoExpire(_handle, expiredInSeconds);
  }

  /// Disable auto key expiration. This is a downgrade operation.
  bool disableAutoKeyExpire() {
    return _disableAutoExpire(_handle);
  }

  /// Enable compare value before update/insert.
  bool enableCompareBeforeSet() {
    return _enableCompareBeforeSet(_handle);
  }

  /// Disable compare value before update/insert.
  bool disableCompareBeforeSet() {
    return _disableCompareBeforeSet(_handle);
  }

  /// remove the storage of the MMKV, including the data file & meta file (.crc)
  /// Note: the existing instance (if any) will be closed & destroyed
  static bool removeStorage(String mmapID, {String? rootDir}) {
    final mmapIDPtr = mmapID.toNativeUtf8();
    final rootDirPtr = _string2Pointer(rootDir);

    final ret = _removeStorage(mmapIDPtr, rootDirPtr);

    calloc.free(mmapIDPtr);
    calloc.free(rootDirPtr);

    return _int2Bool(ret);
  }

  void checkContentChangedByOuterProcess() {

  }
}

void _logRedirect(int logLevel, Pointer<Utf8> file, int line, Pointer<Utf8> funcname, Pointer<Utf8> message) {
  if (_mmkvPlatform.theHandler == null) {
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

  _mmkvPlatform.theHandler?.mmkvLog(level, _pointer2String(file)!, line, _pointer2String(funcname)!, _pointer2String(message)!);
}

enum _MMKVErrorType {
  MMKVCRCCheckFail,
  MMKVFileLength,
}

int _errorHandler(Pointer<Utf8> mmapIDPtr, int errorType) {
  if (_mmkvPlatform.theHandler == null || mmapIDPtr == nullptr) {
    return MMKVRecoverStrategic.OnErrorDiscard.index;
  }

  final mmapID = _pointer2String(mmapIDPtr);
  MMKVRecoverStrategic strategic;
  if (errorType == _MMKVErrorType.MMKVCRCCheckFail.index) {
    strategic = _mmkvPlatform.theHandler!.onMMKVCRCCheckFail(mmapID!);
  } else if (errorType == _MMKVErrorType.MMKVFileLength.index) {
    strategic = _mmkvPlatform.theHandler!.onMMKVFileLengthError(mmapID!);
  } else {
    strategic = MMKVRecoverStrategic.OnErrorDiscard;
  }

  return strategic.index;
}

void _contentChangeHandler(Pointer<Utf8> mmapIDPtr) {
  if (_mmkvPlatform.theHandler != null && mmapIDPtr != nullptr) {
    final handler = _mmkvPlatform.theHandler!;
    if (handler.wantContentChangeNotification()) {
      final mmapID = _pointer2String(mmapIDPtr);
      handler.onContentChangedByOuterProcess(mmapID!);
    }
  }
}

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

final MMKVPluginPlatform _mmkvPlatform = MMKVPluginPlatform.instance!;

final Pointer<Void> Function(Pointer<Utf8> mmapID, int, Pointer<Utf8> cryptKey, Pointer<Utf8> rootDir, int expectedCapacity) _getMMKVWithID =
    _mmkvPlatform.getMMKVWithIDFunc();

final Pointer<Void> Function(int, Pointer<Utf8> cryptKey) _getDefaultMMKV = _mmkvPlatform.getDefaultMMKVFunc();

final Pointer<Utf8> Function(Pointer<Void>) _mmapID = _mmkvPlatform.mmapIDFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeBool = _mmkvPlatform.encodeBoolFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, int, int) _encodeBoolV2 = _mmkvPlatform.encodeBoolV2Func();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeBool = _mmkvPlatform.decodeBoolFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeInt32 = _mmkvPlatform.encodeInt32Func();

final int Function(Pointer<Void>, Pointer<Utf8>, int, int) _encodeInt32V2 = _mmkvPlatform.encodeInt32V2Func();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeInt32 = _mmkvPlatform.decodeInt32Func();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _encodeInt64 = _mmkvPlatform.encodeInt64Func();

final int Function(Pointer<Void>, Pointer<Utf8>, int, int) _encodeInt64V2 = _mmkvPlatform.encodeInt64V2Func();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _decodeInt64 = _mmkvPlatform.decodeInt64Func();

final int Function(Pointer<Void>, Pointer<Utf8>, double) _encodeDouble = _mmkvPlatform.encodeDoubleFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, double, int) _encodeDoubleV2 = _mmkvPlatform.encodeDoubleV2Func();

final double Function(Pointer<Void>, Pointer<Utf8>, double) _decodeDouble = _mmkvPlatform.decodeDoubleFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int) _encodeBytes =  _mmkvPlatform.encodeBytesFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int, int) _encodeBytesV2 =  _mmkvPlatform.encodeBytesV2Func();

final Pointer<Uint8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint64>) _decodeBytes =  _mmkvPlatform.decodeBytesFunc();

final int Function(Pointer<Void>, Pointer<Uint8>, int) _reKey = _mmkvPlatform.reKeyFunc();

final Pointer<Uint8> Function(Pointer<Void>, Pointer<Uint64>) _cryptKey = _mmkvPlatform.cryptKeyFunc();

final void Function(Pointer<Void>, Pointer<Uint8>, int) _checkReSetCryptKey = _mmkvPlatform.checkReSetCryptKeyFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, int) _valueSize = _mmkvPlatform.valueSizeFunc();

final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Void>, int) _writeValueToNB =  _mmkvPlatform.writeValueToNBFunc();

final int Function(Pointer<Void>, Pointer<Pointer<Pointer<Utf8>>>, Pointer<Pointer<Uint32>>, int) _allKeys =  _mmkvPlatform.allKeysFunc();
final int Function(Pointer<Void>, Pointer<Utf8>) _containsKey = _mmkvPlatform.containsKeyFunc();

final int Function(Pointer<Void>, int) _count = _mmkvPlatform.countFunc();

final int Function(Pointer<Void>) _totalSize = _mmkvPlatform.totalSizeFunc();

final int Function(Pointer<Void>) _actualSize = _mmkvPlatform.actualSizeFunc();

final void Function(Pointer<Void>, Pointer<Utf8>) _removeValueForKey = _mmkvPlatform.removeValueForKeyFunc();

final void Function(Pointer<Void>, Pointer<Pointer<Utf8>>, Pointer<Uint32>, int) _removeValuesForKeys =  _mmkvPlatform.removeValuesForKeysFunc();
final void Function(Pointer<Void>, int) _clearAll = _mmkvPlatform.clearAllFunc();

final void Function(Pointer<Void>, int) _mmkvSync =  _mmkvPlatform.mmkvSyncFunc();

final void Function(Pointer<Void>) _clearMemoryCache = _mmkvPlatform.clearMemoryCacheFunc();

final int Function() _pageSize =  _mmkvPlatform.pageSizeFunc();

final Pointer<Utf8> Function() _version =  _mmkvPlatform.versionFunc();

final void Function(Pointer<Void>) _trim =  _mmkvPlatform.trimFunc();

final void Function(Pointer<Void>) _mmkvClose =  _mmkvPlatform.mmkvCloseFunc();

final void Function(Pointer<Void>, Pointer<Void>, int) _memcpy = _mmkvPlatform.memcpyFunc();

final int Function(Pointer<Utf8> mmapID, Pointer<Utf8> dstDir, Pointer<Utf8> rootPath) _backupOne = _mmkvPlatform.backupOneFunc();

final int Function(Pointer<Utf8> mmapID, Pointer<Utf8> srcDir, Pointer<Utf8> rootPath) _restoreOne = _mmkvPlatform.restoreOneFunc();

final int Function(Pointer<Utf8> dstDir) _backupAll = _mmkvPlatform.backupAllFunc();
final int Function(Pointer<Utf8> srcDir) _restoreAll = _mmkvPlatform.restoreAllFunc();

final bool Function(Pointer<Void>, int) _enableAutoExpire = _mmkvPlatform.enableAutoExpireFunc();

final bool Function(Pointer<Void>) _disableAutoExpire = _mmkvPlatform.disableAutoExpireFunc();

final bool Function(Pointer<Void>) _enableCompareBeforeSet = _mmkvPlatform.enableCompareBeforeSetFunc();

final bool Function(Pointer<Void>) _disableCompareBeforeSet = _mmkvPlatform.disableCompareBeforeSetFunc();

final int Function(Pointer<Utf8> mmapID, Pointer<Utf8> rootPath) _removeStorage = _mmkvPlatform.removeStorageFunc();

final bool Function(Pointer<Void>) _isMultiProcess = _mmkvPlatform.isMultiProcessFunc();

final bool Function(Pointer<Void>) _isReadOnly = _mmkvPlatform.isReadOnlyFunc();

final ErrorCallbackRegister _registerErrorHandler = _mmkvPlatform.registerErrorHandlerFunc();

final ContentCallbackRegister _registerContentHandler = _mmkvPlatform.registerContentHandlerFunc();

final void Function(Pointer<Void>) _checkContentChanged = _mmkvPlatform.checkContentChangedFunc();
