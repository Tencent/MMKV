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
import { MMKVLogLevel } from './MMKVLogLevel';

/**
 * The recover strategic of MMKV on errors. {@link MMKV#registerHandler}
 */
export enum MMKVRecoverStrategic {
  /**
   * The default strategic is to discard everything on errors.
   */
  OnErrorDiscard,

  /**
   * The recover strategic will try to recover as much data as possible.
   */
  OnErrorRecover,
}

/**
 * Callback handler for MMKV.
 * Callback is called on the operating thread of the MMKV instance.
 */
export abstract class MMKVHandler {
  /**
   * @return Return true if you want log redirecting.
   */
  public wantLogRedirect(): boolean {
    return false;
  }

  /**
   * Log Redirecting.
   * @param level The level of this log.
   * @param file The file name of this log.
   * @param line The line of code of this log.
   * @param function The function name of this log.
   * @param message The content of this log.
   */
  public mmkvLog(level: MMKVLogLevel, file: string, line: number, func: string, message: string): void {
    switch (level) {
      case MMKVLogLevel.LevelDebug:
        hilog.debug(0x0000, 'mmkv', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      case MMKVLogLevel.LevelInfo:
        hilog.info(0x0000, 'mmkv', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      case MMKVLogLevel.LevelWarning:
        hilog.warn(0x0000, 'mmkv', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      case MMKVLogLevel.LevelError:
        hilog.error(0x0000, 'mmkv', '<%{public}s:%{public}d::%{public}s> %{public}s', file, line, func, message);
        break;
      default:
        break;
    }
  }

  /**
   * By default MMKV will discard all data on crc32-check failure. {@link MMKVRecoverStrategic#OnErrorDiscard}
   * @param mmapID The unique ID of the MMKV instance.
   * @return Return {@link MMKVRecoverStrategic#OnErrorRecover} to recover any data on the file.
   */
  public onMMKVCRCCheckFail(mmapID: string): MMKVRecoverStrategic {
    return MMKVRecoverStrategic.OnErrorDiscard;
  }

  /**
   * By default MMKV will discard all data on file length mismatch. {@link MMKVRecoverStrategic#OnErrorDiscard}
   * @param mmapID The unique ID of the MMKV instance.
   * @return Return {@link MMKVRecoverStrategic#OnErrorRecover} to recover any data on the file.
   */
  public onMMKVFileLengthError(mmapID: string): MMKVRecoverStrategic {
    return MMKVRecoverStrategic.OnErrorDiscard;
  }

  /**
   * @return Return true to enable inter-process content change notification
   */
  public wantContentChangeNotification(): boolean {
    return false;
  }

  /**
   * Inter-process content change notification.
   * Triggered by any method call, such as getXXX() or setXXX() or {@link MMKV#checkContentChangedByOuterProcess()}.
   * @param mmapID The unique ID of the changed MMKV instance.
   */
  public onContentChangedByOuterProcess(mmapID: string): void {
    return;
  }
}
