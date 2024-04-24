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

import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { MMKV, MMKVLogLevel } from '@tencent/mmkv';
import { GlobalContainer } from '../Util/Util';
import childProcessManager from '@ohos.app.ability.childProcessManager';
import { DemoProcess } from '../process/DemoProcess';
import { BusinessError } from '@ohos.base';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(0x0000, 'mmkvdemo', '%{public}s', 'Ability onCreate');

    let appCtx = this.context.getApplicationContext();
    let result = MMKV.initialize(appCtx/*, MMKVLogLevel.LevelNone*/);
    hilog.info(0x0000, 'mmkvdemo', 'rootDir: %{public}s', result);

    // share between pages
    GlobalContainer.setObject('context', appCtx);

    // looks like there's noway to test multi-process for the time being
    // this.startChildProcess();
  }

  onDestroy(): void {
    hilog.info(0x0000, 'mmkvdemo', '%{public}s', 'Ability onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    hilog.info(0x0000, 'mmkvdemo', '%{public}s', 'Ability onWindowStageCreate');

    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(0x0000, 'mmkvdemo', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(0x0000, 'mmkvdemo', 'Succeeded in loading the content.');
    });
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(0x0000, 'mmkvdemo', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(0x0000, 'mmkvdemo', '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(0x0000, 'mmkvdemo', '%{public}s', 'Ability onBackground');
  }

  startChildProcess(): void {
    try {
      // 这里要调用下DemoProcess类的任意方法，防止没有引用到而被构建工具优化掉
      hilog.info(0x0000, 'mmkvdemo', 'startChildProcess %{public}s', DemoProcess.toString());
      childProcessManager.startChildProcess("./ets/process/DemoProcess.ts", childProcessManager.StartMode.APP_SPAWN_FORK, (err, data) => {
        if (data) {
          console.log(`startChildProcess success, pid: ${data}`);
        } else {
          console.error(`startChildProcess error, errorCode: ${err.code}`);
        }
      });
    } catch (err) {
      console.error(`startChildProcess error, errorCode: ${(err as BusinessError).code}`);
    }
  }
}
