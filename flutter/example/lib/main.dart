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
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:mmkv/mmkv.dart';

void main() async {
  // must wait for MMKV to finish initialization
  final rootDir = await MMKV.initialize();
  print('MMKV for flutter with rootDir = $rootDir');

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
          title: const Text('MMKV Plugin example app'),
        ),
        body: Center(
            child: Column(children: <Widget>[
          Text('MMKV Version: ${MMKV.version}\n'),
          TextButton(
              onPressed: () {
                functionalTest();
              },
              child: Text('Functional Test', style: TextStyle(fontSize: 18))),
          TextButton(
              onPressed: () {
                testReKey();
              },
              child: Text('Encryption Test', style: TextStyle(fontSize: 18))),
        ])),
      ),
    );
  }

  void functionalTest() {
    /* Note: If you come across to failing to load defaultMMKV() on Android after upgrading Flutter from 1.20+ to 2.0+,
     * you can try passing this encryption key '\u{2}U' instead.
     * var mmkv = MMKV.defaultMMKV(cryptKey: '\u{2}U');
     */
    var mmkv = MMKV.defaultMMKV();
    mmkv.encodeBool('bool', true);
    print('bool = ${mmkv.decodeBool('bool')}');

    mmkv.encodeInt32('int32', (1 << 31) - 1);
    print('max int32 = ${mmkv.decodeInt32('int32')}');

    mmkv.encodeInt32('int32', 0 - (1 << 31));
    print('min int32 = ${mmkv.decodeInt32('int32')}');

    mmkv.encodeInt('int', (1 << 63) - 1);
    print('max int = ${mmkv.decodeInt('int')}');

    mmkv.encodeInt('int', 0 - (1 << 63));
    print('min int = ${mmkv.decodeInt('int')}');

    mmkv.encodeDouble('double', double.maxFinite);
    print('max double = ${mmkv.decodeDouble('double')}');

    mmkv.encodeDouble('double', double.minPositive);
    print('min positive double = ${mmkv.decodeDouble('double')}');

    String str = 'Hello dart from MMKV';
    mmkv.encodeString('string', str);
    print('string = ${mmkv.decodeString('string')}');

    mmkv.encodeString('string', '');
    print('empty string = ${mmkv.decodeString('string')}');
    print('contains "string": ${mmkv.containsKey('string')}');

    mmkv.encodeString('string', null);
    print('null string = ${mmkv.decodeString('string')}');
    print('contains "string": ${mmkv.containsKey('string')}');

    str += ' with bytes';
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(str))!;
    mmkv.encodeBytes('bytes', bytes);
    bytes.destroy();
    bytes = mmkv.decodeBytes('bytes')!;
    print('bytes = ${Utf8Decoder().convert(bytes.asList()!)}');
    bytes.destroy();

    print('contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValue('bool');
    print('after remove, contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValues(['int32', 'int']);
    print('all keys: ${mmkv.allKeys}');

    mmkv.trim();
    mmkv.clearMemoryCache();
    print('all keys: ${mmkv.allKeys}');
    mmkv.clearAll();
    print('all keys: ${mmkv.allKeys}');
    // mmkv.sync(true);
    // mmkv.close();
  }

  MMKV testMMKV(
      String mmapID, String? cryptKey, bool decodeOnly, String? rootPath) {
    final mmkv = MMKV(mmapID, cryptKey: cryptKey, rootDir: rootPath);

    if (!decodeOnly) {
      mmkv.encodeBool('bool', true);
    }
    print('bool = ${mmkv.decodeBool('bool')}');

    if (!decodeOnly) {
      mmkv.encodeInt32('int32', (1 << 31) - 1);
    }
    print('max int32 = ${mmkv.decodeInt32('int32')}');

    if (!decodeOnly) {
      mmkv.encodeInt32('int32', 0 - (1 << 31));
    }
    print('min int32 = ${mmkv.decodeInt32('int32')}');

    if (!decodeOnly) {
      mmkv.encodeInt('int', (1 << 63) - 1);
    }
    print('max int = ${mmkv.decodeInt('int')}');

    if (!decodeOnly) {
      mmkv.encodeInt('int', 0 - (1 << 63));
    }
    print('min int = ${mmkv.decodeInt('int')}');

    if (!decodeOnly) {
      mmkv.encodeDouble('double', double.maxFinite);
    }
    print('max double = ${mmkv.decodeDouble('double')}');

    if (!decodeOnly) {
      mmkv.encodeDouble('double', double.minPositive);
    }
    print('min positive double = ${mmkv.decodeDouble('double')}');

    String str = 'Hello dart from MMKV';
    if (!decodeOnly) {
      mmkv.encodeString('string', str);
    }
    print('string = ${mmkv.decodeString('string')}');

    str += ' with bytes';
    var bytes = MMBuffer.fromList(Utf8Encoder().convert(str))!;
    if (!decodeOnly) {
      mmkv.encodeBytes('bytes', bytes);
    }
    bytes.destroy();
    bytes = mmkv.decodeBytes('bytes')!;
    print('bytes = ${Utf8Decoder().convert(bytes.asList()!)}');
    bytes.destroy();

    print('contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValue('bool');
    print('after remove, contains "bool": ${mmkv.containsKey('bool')}');
    mmkv.removeValues(['int32', 'int']);
    print('all keys: ${mmkv.allKeys}');

    return mmkv;
  }

  void testReKey() {
    final mmapID = 'testAES_reKey1';
    MMKV kv = testMMKV(mmapID, null, false, null);

    kv.reKey("Key_seq_1");
    kv.clearMemoryCache();
    testMMKV(mmapID, 'Key_seq_1', true, null);

    kv.reKey('Key_seq_2');
    kv.clearMemoryCache();
    testMMKV(mmapID, 'Key_seq_2', true, null);

    kv.reKey(null);
    kv.clearMemoryCache();
    testMMKV(mmapID, null, true, null);
  }
}
