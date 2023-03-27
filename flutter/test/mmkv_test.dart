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

import "package:mmkv/mmkv.dart";
import "package:test/test.dart";

void main() {
  test("Int value should be written and read", () {
    final mmkv = MMKV.defaultMMKV();

    mmkv.encodeInt("int", 1024);

    expect(mmkv.decodeInt("int"), 1024);
  });

  test("String value should be written and read", () {
    final mmkv = MMKV.defaultMMKV();

    mmkv.encodeString("string", "test");

    expect(mmkv.decodeInt("string"), "test");
  });

  test("bool value should be written and read", () {
    final mmkv = MMKV.defaultMMKV();

    mmkv.encodeBool("bool", true);

    expect(mmkv.decodeBool("bool"), true);
  });
}
