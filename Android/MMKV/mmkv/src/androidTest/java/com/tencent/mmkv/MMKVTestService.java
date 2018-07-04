package com.tencent.mmkv;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Process;
import android.support.annotation.Nullable;

public class MMKVTestService extends Service {
    public static final String SharedMMKVID = "SharedMMKVID";
    public static final String SharedMMKVKey = "SharedMMKVKey";

    public static final String CMD_Key = "CMD_Key";
    public static final String CMD_Update = "CMD_Update";
    public static final String CMD_Lock = "CMD_Lock";
    public static final String CMD_Kill = "CMD_Kill";

    @Override
    public void onCreate() {
        super.onCreate();

        MMKV.initialize(this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        MMKV mmkv = MMKV.mmkvWithID(SharedMMKVID, MMKV.MULTI_PROCESS_MODE);

        String cmd = intent.getStringExtra(CMD_Key);
        switch (cmd) {
            case CMD_Update:
                int value = mmkv.decodeInt(SharedMMKVKey);
                value += 1;
                mmkv.encode(SharedMMKVKey, value);
                break;
            case CMD_Lock:
                mmkv.lock();
                break;
            case CMD_Kill:
                stopSelf();
                break;
        }

        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        Process.killProcess(Process.myPid());
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
