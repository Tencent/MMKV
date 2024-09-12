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

import { MMKVLogLevel, MMKVHandler, MMKVRecoverStrategic } from '@tencent/mmkv';
import { hilog } from '@kit.PerformanceAnalysisKit';

export class MyMMKVHandler extends MMKVHandler {
  /*private uniqueId: number;

  constructor() {
    super();
    this.uniqueId = Math.round(Math.random() * Number.MAX_SAFE_INTEGER);
    hilog.info(0x0000, 'mmkvdemo', 'uniqueId: %{public}d', this.uniqueId);
  }*/

  public wantLogRedirect(): boolean {
    let result = true;
    // hilog.info(0x0000, 'mmkvdemo', 'uniqueId: %{public}d, wantLogRedirect: %{public}s', this.uniqueId, result);
    return result;
  }

  public mmkvLog(level: MMKVLogLevel, file: string, line: number, func: string, message: string): void {
    switch (level) {
      case MMKVLogLevel.LevelDebug:
        hilog.debug(0x0000, 'mmkv-redirect', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      case MMKVLogLevel.LevelInfo:
        hilog.info(0x0000, 'mmkv-redirect', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      case MMKVLogLevel.LevelWarning:
        hilog.warn(0x0000, 'mmkv-redirect', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      case MMKVLogLevel.LevelError:
        hilog.error(0x0000, 'mmkv-redirect', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      default:
        break;
    }
  }

  public onMMKVCRCCheckFail(mmapID: string): MMKVRecoverStrategic {
    hilog.warn(0x0000, 'mmkvdemo', 'onMMKVCRCCheckFail: %{public}s', mmapID);
    return MMKVRecoverStrategic.OnErrorRecover;
  }

  public onMMKVFileLengthError(mmapID: string): MMKVRecoverStrategic {
    hilog.warn(0x0000, 'mmkvdemo', 'onMMKVFileLengthError: %{public}s', mmapID);
    return MMKVRecoverStrategic.OnErrorRecover;
  }

  public wantContentChangeNotification(): boolean {
    // hilog.info(0x0000, 'mmkvdemo', 'wantContentChangeNotification uniqueId: %{public}d', this.uniqueId);
    return true;
  }

  public onContentChangedByOuterProcess(mmapID: string): void {
    hilog.info(0x0000, 'mmkvdemo', 'onContentChangedByOuterProcess: %{public}s', mmapID);
  }
}
