import { appTasks } from '@ohos/hvigor-ohos-plugin';
import signData from './sign-config.json';

export default {
    system: appTasks, /* Built-in plugin of Hvigor. It cannot be modified. */
    plugins: [], /* Custom plugin to extend the functionality of Hvigor. */
    config: {
        ohos: {
            overrides: {
                signingConfig: {
                    type: "HarmonyOS",
                    material: signData,
                }
            }
        }
    }
}
