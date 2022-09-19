package com.tencent.mmkvdemo;

import android.app.Application;
import android.util.Log;
import com.getkeepsafe.relinker.ReLinker;
import com.tencent.mmkv.MMKV;
import com.tencent.mmkv.MMKVContentChangeNotification;
import com.tencent.mmkv.MMKVHandler;
import com.tencent.mmkv.MMKVLogLevel;
import com.tencent.mmkv.MMKVRecoverStrategic;

public class MyApplication extends Application implements MMKVHandler, MMKVContentChangeNotification {
    @Override
    public void onCreate() {
        super.onCreate();

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
    public void onContentChangedByOuterProcess(String mmapID) {
        Log.i("MMKV", "other process has changed content of : " + mmapID);
    }
}
