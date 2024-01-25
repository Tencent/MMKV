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
import "dart:io";
import "dart:typed_data";

import "package:flutter/material.dart";
import "package:mmkv/mmkv.dart";
import "package:path_provider_foundation/path_provider_foundation.dart";

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  var groupDir = null;
  if (Platform.isIOS) {
    final PathProviderFoundation provider = PathProviderFoundation();
    groupDir = await provider.getContainerPath(appGroupIdentifier: "group.com.lingol.mmkvdemo");
  }

  // must wait for MMKV to finish initialization
  final rootDir = await MMKV.initialize(groupDir: groupDir);
  print("MMKV for flutter with rootDir = $rootDir");

  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  @override
  void initState() {
    super.initState();
    initPlatformState();
  }

  // Platform messages are asynchronous, so we initialize in an async method.
  Future<void> initPlatformState() async {
    // If the widget was removed from the tree while the asynchronous platform
    // message was in flight, we want to discard the reply rather than calling
    // setState to update our non-existent appearance.
    if (!mounted) return;

    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text("MMKV Plugin example app"),
        ),
        body: Center(
            child: Column(children: <Widget>[
          Text("MMKV Version: ${MMKV.version}\n"),
          TextButton(
              onPressed: () {
                functionalTest();
              },
              child: Text("Functional Test", style: TextStyle(fontSize: 18))),
          TextButton(
              onPressed: () {
                testReKey();
              },
              child: Text("Encryption Test", style: TextStyle(fontSize: 18))),
          TextButton(
              onPressed: () {
                testAutoExpire();
              },
              child: Text("Auto Expiration Test", style: TextStyle(fontSize: 18))),
          TextButton(
              onPressed: () {
                testCompareBeforeSet();
              },
              child: Text("Compare Before Insert/Update Test", style: TextStyle(fontSize: 18))),
          TextButton(
              onPressed: () {
                testBackup();
                testRestore();
              },
              child: Text("Backup & Restore Test", style: TextStyle(fontSize: 18))),
          TextButton(
              onPressed: () {
                testRemoveStorage();
              },
              child: Text("Remove Storage Test", style: TextStyle(fontSize: 18))),
        ])),
      ),
    );
  }

  void functionalTest() {
    /* Note: If you come across to failing to load defaultMMKV() on Android after upgrading Flutter from 1.20+ to 2.0+,
     * you can try passing this encryption key '\u{2}U' instead.
     * var mmkv = MMKV.defaultMMKV(cryptKey: '\u{2}U');
     */
    final mmkv = MMKV.defaultMMKV();
    mmkv.encodeBool("bool", true);
    print('bool = ${mmkv.decodeBool('bool')}');

    mmkv.encodeInt32("int32", (1 << 31) - 1);
    print('max int32 = ${mmkv.decodeInt32('int32')}');

    mmkv.encodeInt32("int32", 0 - (1 << 31));
    print('min int32 = ${mmkv.decodeInt32('int32')}');

    mmkv.encodeInt("int", (1 << 63) - 1);
    print('max int = ${mmkv.decodeInt('int')}');

    mmkv.encodeInt("int", 0 - (1 << 63));
    print('min int = ${mmkv.decodeInt('int')}');

    mmkv.encodeDouble("double", double.maxFinite);
    print('max double = ${mmkv.decodeDouble('double')}');

    mmkv.encodeDouble("double", double.minPositive);
    print('min positive double = ${mmkv.decodeDouble('double')}');

    String str = "Hello dart from MMKV";
    mmkv.encodeString("string", str);
    print('string = ${mmkv.decodeString('string')}');

    mmkv.encodeString("string", "");
    print('empty string = ${mmkv.decodeString('string')}');
    print('contains "string": ${mmkv.containsKey('string')}');

    mmkv.encodeString("string", null);
    print('null string = ${mmkv.decodeString('string')}');
    print('contains "string": ${mmkv.containsKey('string')}');

    str += " with bytes";
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(str))!;
    mmkv.encodeBytes("bytes", bytes);
    bytes.destroy();
    bytes = mmkv.decodeBytes("bytes")!;
    print("bytes = ${Utf8Decoder().convert(bytes.asList()!)}");
    bytes.destroy();

    // test empty bytes
    {
      final list = Uint8List(0);
      final buffer = MMBuffer.fromList(list)!;

      const key = "empty_bytes";
      mmkv.encodeBytes(key, buffer);
      buffer.destroy();

      String bytesA;
      final result = mmkv.decodeBytes(key);
      if (result == null) {
        bytesA = "decodeBytes is null";
      } else {
        // bytes = result.takeList().toString();
        final listA = result.asList();
        bytesA = listA.toString();
        result.destroy();
      }
      print("$key = $bytesA");
    }

    print('contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValue("bool");
    print('after remove, contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValues(["int32", "int"]);
    print("all keys: ${mmkv.allKeys}");

    mmkv.trim();
    mmkv.clearMemoryCache();
    print("all keys: ${mmkv.allKeys}");
    mmkv.clearAll();
    print("all keys: ${mmkv.allKeys}");
    // mmkv.sync(true);
    // mmkv.close();
  }

  MMKV testMMKV(String mmapID, String? cryptKey, bool decodeOnly, String? rootPath) {
    final mmkv = MMKV(mmapID, cryptKey: cryptKey, rootDir: rootPath);

    if (!decodeOnly) {
      mmkv.encodeBool("bool", true);
    }
    print('bool = ${mmkv.decodeBool('bool')}');

    if (!decodeOnly) {
      mmkv.encodeInt32("int32", (1 << 31) - 1);
    }
    print('max int32 = ${mmkv.decodeInt32('int32')}');

    if (!decodeOnly) {
      mmkv.encodeInt32("int32", 0 - (1 << 31));
    }
    print('min int32 = ${mmkv.decodeInt32('int32')}');

    if (!decodeOnly) {
      mmkv.encodeInt("int", (1 << 63) - 1);
    }
    print('max int = ${mmkv.decodeInt('int')}');

    if (!decodeOnly) {
      mmkv.encodeInt("int", 0 - (1 << 63));
    }
    print('min int = ${mmkv.decodeInt('int')}');

    if (!decodeOnly) {
      mmkv.encodeDouble("double", double.maxFinite);
    }
    print('max double = ${mmkv.decodeDouble('double')}');

    if (!decodeOnly) {
      mmkv.encodeDouble("double", double.minPositive);
    }
    print('min positive double = ${mmkv.decodeDouble('double')}');

    String str = "Hello dart from MMKV";
    if (!decodeOnly) {
      mmkv.encodeString("string", str);
    }
    print('string = ${mmkv.decodeString('string')}');

    str += " with bytes";
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(str))!;
    if (!decodeOnly) {
      mmkv.encodeBytes("bytes", bytes);
    }
    bytes.destroy();
    bytes = mmkv.decodeBytes("bytes")!;
    print("bytes = ${Utf8Decoder().convert(bytes.asList()!)}");
    bytes.destroy();

    print('contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValue("bool");
    print('after remove, contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValues(["int32", "int"]);
    print("all keys: ${mmkv.allKeys}");

    return mmkv;
  }

  void testReKey() {
    final mmapID = "testAES_reKey1";
    final MMKV kv = testMMKV(mmapID, null, false, null);

    kv.reKey("Key_seq_1");
    kv.clearMemoryCache();
    testMMKV(mmapID, "Key_seq_1", true, null);

    kv.reKey("Key_seq_2");
    kv.clearMemoryCache();
    testMMKV(mmapID, "Key_seq_2", true, null);

    kv.reKey(null);
    kv.clearMemoryCache();
    testMMKV(mmapID, null, true, null);
  }

  void testBackup() {
    final rootDir = FileSystemEntity.parentOf(MMKV.rootDir);
    var backupRootDir = rootDir + "/mmkv_backup_3";
    final String mmapID = "test/AES";
    final String cryptKey = "Tencent MMKV";
    final String otherDir = rootDir + "/mmkv_3";
    testMMKV(mmapID, cryptKey, false, otherDir);

    final ret = MMKV.backupOneToDirectory(mmapID, backupRootDir, rootDir: otherDir);
    print("backup one [$mmapID] return: $ret");

    backupRootDir = rootDir + "/mmkv_backup";
    final count = MMKV.backupAllToDirectory(backupRootDir);
    print("backup all count: $count");
  }

  void testRestore() {
    final rootDir = FileSystemEntity.parentOf(MMKV.rootDir);
    var backupRootDir = rootDir + "/mmkv_backup_3";
    final String mmapID = "test/AES";
    final String cryptKey = "Tencent MMKV";
    final String otherDir = rootDir + "/mmkv_3";

    final kv = MMKV(mmapID, cryptKey: cryptKey, rootDir: otherDir);
    kv.encodeString("test_restore", "value before restore");
    print("before restore [${kv.mmapID}] allKeys: ${kv.allKeys}");
    final ret = MMKV.restoreOneMMKVFromDirectory(mmapID, backupRootDir, rootDir: otherDir);
    print("restore one [${kv.mmapID}] ret = $ret");
    if (ret) {
      print("after restore [${kv.mmapID}] allKeys: ${kv.allKeys}");
    }

    backupRootDir = rootDir + "/mmkv_backup";
    final count = MMKV.restoreAllFromDirectory(backupRootDir);
    print("restore all count $count");
    if (count > 0) {
      var mmkv = MMKV.defaultMMKV();
      print("check on restore file[${mmkv.mmapID}] allKeys: ${mmkv.allKeys}");

      mmkv = MMKV("testAES_reKey1");
      print("check on restore file[${mmkv.mmapID}] allKeys: ${mmkv.allKeys}");
    }
  }

  void testAutoExpire() {
    final mmkv = MMKV("test_auto_expire");
    mmkv.clearAll(keepSpace: true);
    mmkv.disableAutoKeyExpire();

    mmkv.enableAutoKeyExpire(1);
    mmkv.encodeBool("auto_expire_key_1", true);
    mmkv.encodeInt32("auto_expire_key_2", 1, 1);
    mmkv.encodeInt("auto_expire_key_3", 2, 1);
    mmkv.encodeDouble("auto_expire_key_4", 3.0, 1);
    mmkv.encodeString("auto_expire_key_5", "hello auto expire", 1);
    {
      final bytes = MMBuffer.fromList(Utf8Encoder().convert("hello auto expire"))!;
      mmkv.encodeBytes("auto_expire_key_6", bytes, 1);
      bytes.destroy();
    }
    mmkv.encodeBool("never_expire_key_1", true, MMKV.ExpireNever);

    final duration = const Duration(seconds: 2);
    sleep(duration);

    print("auto_expire_key_1: ${mmkv.containsKey("auto_expire_key_1")}");
    print("auto_expire_key_2: ${mmkv.containsKey("auto_expire_key_2")}");
    print("auto_expire_key_3: ${mmkv.containsKey("auto_expire_key_3")}");
    print("auto_expire_key_4: ${mmkv.containsKey("auto_expire_key_4")}");
    print("auto_expire_key_5: ${mmkv.containsKey("auto_expire_key_5")}");
    print("auto_expire_key_6: ${mmkv.containsKey("auto_expire_key_6")}");
    print("never_expire_key_1: ${mmkv.containsKey("never_expire_key_1")}");

    print("count non-expired keys: ${mmkv.countNonExpiredKeys}");
    print("all non-expired keys: ${mmkv.allNonExpiredKeys}");
  }

  void testCompareBeforeSet() {
    final mmkv = MMKV("testCompareBeforeSet", expectedCapacity: 4096 * 2);
    mmkv.enableCompareBeforeSet();

    mmkv.encodeString("key", "extra");

    {
      final String key = "int";
      final int v = 12345;
      mmkv.encodeInt32(key, v);
      final actualSize = mmkv.actualSize;
      print("testCompareBeforeSet actualSize = $actualSize");
      print("testCompareBeforeSet v = ${mmkv.decodeInt(key)}");
      mmkv.encodeInt32(key, v);
      final actualSize2 = mmkv.actualSize;
      print("testCompareBeforeSet actualSize = $actualSize2");
      if (actualSize2 != actualSize) {
        print("testCompareBeforeSet fail");
      }

      mmkv.encodeInt32(key, v * 23);
      print("testCompareBeforeSet actualSize = ${mmkv.actualSize}");
      print("testCompareBeforeSet v = ${mmkv.decodeInt32(key)}");
    }

    {
      final key = "string";
      final v = "w012A🏊🏻good";
      mmkv.encodeString(key, v);
      final actualSize = mmkv.actualSize;
      print("testCompareBeforeSet actualSize = $actualSize");
      print("testCompareBeforeSet v = ${mmkv.decodeString(key)}");
      mmkv.encodeString(key, v);
      final actualSize2 = mmkv.actualSize;
      print("testCompareBeforeSet actualSize = $actualSize2");
      if (actualSize2 != actualSize) {
        print("testCompareBeforeSet fail");
      }

      mmkv.encodeString(key, "temp data 👩🏻‍🏫");
      print("testCompareBeforeSet actualSize = ${mmkv.actualSize}");
      print("testCompareBeforeSet v = ${mmkv.decodeString(key)}");
    }
  }

  void testRemoveStorage() {
    var mmapID = "test_remove";
    {
      final mmkv = MMKV(mmapID, mode: MMKVMode.MULTI_PROCESS_MODE);
      mmkv.encodeBool("bool", true);
    }
    MMKV.removeStorage(mmapID);
    {
      final mmkv = MMKV(mmapID, mode: MMKVMode.MULTI_PROCESS_MODE);
      if (mmkv.count != 0) {
        print("storage not successfully removed");
      }
    }

    mmapID = "test_remove/sg";
    final rootDir = MMKV.rootDir + "_sg";
    var mmkv = MMKV(mmapID, rootDir: rootDir);
    mmkv.encodeBool("bool", true);
    MMKV.removeStorage(mmapID, rootDir: rootDir);
    mmkv = MMKV(mmapID, rootDir: rootDir);
    if (mmkv.count != 0) {
      print("storage not successfully removed");
    }
  }
}
