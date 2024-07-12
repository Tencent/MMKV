import {
  FlutterPlugin,
  FlutterPluginBinding,
  MethodCall,
  MethodCallHandler,
  MethodChannel,
  MethodResult,
} from '@ohos/flutter_ohos';
import { MMKV } from '@tencent/mmkv';
// import { MMKV } from 'mmkv_internal';

import { hilog } from '@kit.PerformanceAnalysisKit';
import common from '@ohos.app.ability.common';

/** MMKVPlugin **/
export default class MMKVPlugin implements FlutterPlugin, MethodCallHandler {
  private channel: MethodChannel | null = null;
  private context: common.Context | null = null;

  constructor(context?: common.Context) {
    // hilog.info(0x0000, 'start', 'MMKVPlugin constructor: %{public}s', context);
    if (context) {
      this.context = context;
    }
  }

  getUniqueClassName(): string {
    return "MMKVPlugin"
  }

  onAttachedToEngine(binding: FlutterPluginBinding): void {
    this.channel = new MethodChannel(binding.getBinaryMessenger(), "mmkv");
    this.channel.setMethodCallHandler(this)

    this.context = binding.getApplicationContext();
    // hilog.info(0x0000, 'start', 'MMKVPlugin onAttachedToEngine: %{public}s', this.context);
  }

  onDetachedFromEngine(binding: FlutterPluginBinding): void {
    if (this.channel != null) {
      this.channel.setMethodCallHandler(null)
    }
  }

  onMethodCall(call: MethodCall, result: MethodResult): void {
    // hilog.info(0x0000, 'start', 'onMethodCall: %{public}s', call.method);
    if (call.method == "initializeMMKV") {
      let rootDir: string = call.argument("rootDir");
      let cacheDir: string = call.argument("cacheDir");
      let logLevel: number = call.argument("logLevel");
      let ret: string = MMKV.initializeWithPath(rootDir, cacheDir, logLevel);
      result.success(ret);
    } else if (call.method == "getApplicationDocumentsPath") {
      let ret: string = this.getApplicationDocumentsPath();
      result.success(ret);
    } else if (call.method == "getTemporaryPath") {
      let ret: string = this.getTemporaryPath();
      result.success(ret);
    } else {
      result.notImplemented()
    }
  }

  private getApplicationDocumentsPath(): string {
    // return PathUtils.getDataDirectory(this.context) ?? "";
    if (this.context) {
      return this.context.filesDir;
    }
    return "";
  }

  private getTemporaryPath(): string {
    if (this.context) {
      return this.context.cacheDir;
    }
    return "";
  }
}