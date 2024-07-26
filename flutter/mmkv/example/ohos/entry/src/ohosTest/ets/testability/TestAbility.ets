/*
* Copyright (c) 2023 Hunan OpenValley Digital Industry Development Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

import UIAbility from '@ohos.app.ability.UIAbility';
import AbilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';
import hilog from '@ohos.hilog';
import { Hypium } from '@ohos/hypium';
import testsuite from '../test/List.test';
import window from '@ohos.window';

export default class TestAbility extends UIAbility {
    onCreate(want, launchParam) {
        hilog.info(0x0000, 'testTag', '%{public}s', 'TestAbility onCreate');
        hilog.info(0x0000, 'testTag', '%{public}s', 'want param:' + JSON.stringify(want) ?? '');
        hilog.info(0x0000, 'testTag', '%{public}s', 'launchParam:'+ JSON.stringify(launchParam) ?? '');
        var abilityDelegator: any
        abilityDelegator = AbilityDelegatorRegistry.getAbilityDelegator()
        var abilityDelegatorArguments: any
        abilityDelegatorArguments = AbilityDelegatorRegistry.getArguments()
        hilog.info(0x0000, 'testTag', '%{public}s', 'start run testcase!!!');
        Hypium.hypiumTest(abilityDelegator, abilityDelegatorArguments, testsuite)
    }

    onDestroy() {
        hilog.info(0x0000, 'testTag', '%{public}s', 'TestAbility onDestroy');
    }

    onWindowStageCreate(windowStage: window.WindowStage) {
        hilog.info(0x0000, 'testTag', '%{public}s', 'TestAbility onWindowStageCreate');
        windowStage.loadContent('testability/pages/Index', (err, data) => {
            if (err.code) {
                hilog.error(0x0000, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err) ?? '');
                return;
            }
            hilog.info(0x0000, 'testTag', 'Succeeded in loading the content. Data: %{public}s',
                JSON.stringify(data) ?? '');
        });
    }

    onWindowStageDestroy() {
        hilog.info(0x0000, 'testTag', '%{public}s', 'TestAbility onWindowStageDestroy');
    }

    onForeground() {
        hilog.info(0x0000, 'testTag', '%{public}s', 'TestAbility onForeground');
    }

    onBackground() {
        hilog.info(0x0000, 'testTag', '%{public}s', 'TestAbility onBackground');
    }
}