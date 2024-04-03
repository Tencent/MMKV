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

import { hilog } from '@kit.PerformanceAnalysisKit';
import { MMKV, MMKVLogLevel } from 'mmkv';

@Entry
@Component
struct Index {
  @State message: string = 'Functionality Test';

  build() {
    Row() {
      Column({ space: 20 }) {
        Button(this.message)
          .type(ButtonType.Normal)
          .buttonStyle(ButtonStyleMode.TEXTUAL)
          .fontSize(18)
          .onClick(() => {
            let kv = MMKV.defaultMMKV();
            kv.encodeBool('bool', true);
            hilog.info(0, "mmkvdemo", 'decode bool = %{public}s', kv.decodeBool('bool'));
          })
        Text('mmkv version: ' + MMKV.version)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
        Text(MMKV.rootDir)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
      }
      .width('100%')
    }
    .height('100%')
  }
}