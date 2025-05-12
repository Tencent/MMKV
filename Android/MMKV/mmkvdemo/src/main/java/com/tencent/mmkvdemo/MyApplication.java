/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2025 THL A29 Limited, a Tencent company.
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

package com.tencent.mmkvdemo;

import android.app.Application;
import android.util.Log;
import com.getkeepsafe.relinker.ReLinker;
import com.tencent.mmkv.MMKV;
import com.tencent.mmkv.MMKVContentChangeNotification;
import com.tencent.mmkv.MMKVHandler;
import com.tencent.mmkv.MMKVLogLevel;
import com.tencent.mmkv.MMKVRecoverStrategic;
import com.tencent.mmkv.NameSpace;

public class MyApplication extends Application implements MMKVHandler, MMKVContentChangeNotification {
    @Override
    public void onCreate() {
        super.onCreate();

        // NameSpace can access an MMKV instance regardless of MMKV.initialized() been called or not
        testNameSpace();

        // set root dir
        //String rootDir = MMKV.initialize(this);
        String dir = getFilesDir().getAbsolutePath() + "/mmkv";
        String rootDir = MMKV.initialize(this, dir, new MMKV.LibLoader() {
            @Override
            public void loadLibrary(String libName) {
                ReLinker.loadLibrary(MyApplication.this, libName);
            }
        }, MMKVLogLevel.LevelInfo,this);
        Log.i("MMKV", "mmkv root: " + rootDir);

        // set log level
        // MMKV.setLogLevel(MMKVLogLevel.LevelInfo);

        // you can turn off logging
        //MMKV.setLogLevel(MMKVLogLevel.LevelNone);

        // register log redirecting & recover handler is moved into MMKV.initialize()
        // MMKV.registerHandler(this);

        // content change notification
        MMKV.registerContentChangeNotify(this);
    }

    @Override
    public void onTerminate() {
        MMKV.onExit();

        super.onTerminate();
    }

    @Override
    public MMKVRecoverStrategic onMMKVCRCCheckFail(String mmapID) {
        return MMKVRecoverStrategic.OnErrorRecover;
    }

    @Override
    public MMKVRecoverStrategic onMMKVFileLengthError(String mmapID) {
        return MMKVRecoverStrategic.OnErrorRecover;
    }

    @Override
    public boolean wantLogRedirecting() {
        return true;
    }

    @Override
    public void mmkvLog(MMKVLogLevel level, String file, int line, String func, String message) {
        String log = "<" + file + ":" + line + "::" + func + "> " + message;
        switch (level) {
            case LevelDebug:
                Log.d("redirect logging MMKV", log);
                break;
            case LevelNone:
            case LevelInfo:
                Log.i("redirect logging MMKV", log);
                break;
            case LevelWarning:
                Log.w("redirect logging MMKV", log);
                break;
            case LevelError:
                Log.e("redirect logging MMKV", log);
                break;
        }
    }

    @Override
    public native long getNativeLogHandler();

    @Override
    public void onContentChangedByOuterProcess(String mmapID) {
        Log.i("MMKV", "other process has changed content of : " + mmapID);
    }

    private void testNameSpace() {
        final String nsRoot = getFilesDir().getAbsolutePath() + "/namespace";
        final NameSpace ns = MMKV.nameSpace(nsRoot);
        MMKV mmkv = ns.mmkvWithID("test/namespace/crypt", MMKV.MULTI_PROCESS_MODE, "crypt_key", 4096 * 2);
        MainActivity.testMMKV(mmkv, false);

        System.loadLibrary("mmkvdemo");

        long nativeLogger = getNativeLogHandler();
        Log.i("MMKVDemo", "native log handler: " + nativeLogger);

        testNameSpaceInNative(nsRoot, "testNameSpaceInNative");
    }

    private native void testNameSpaceInNative(String nameSpaceRoot, String mmapID);
}
