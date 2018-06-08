package com.tencent.mmkvdemo;

import android.content.Intent;
import android.os.IBinder;
import android.os.Process;
import android.os.ResultReceiver;
import android.support.annotation.Nullable;

import com.tencent.mmkv.MMKV;

public class MyService_1 extends BenchMarkBaseService {
    private static final String CALLER = "MyService_1";

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        System.out.println("MyService_1 onStartCommand");
        String cmd = intent.getStringExtra(CMD_ID);
        if (cmd.equals(CMD_READ_INT)) {
            super.batchReadInt(CALLER);
        } else if (cmd.equals(CMD_READ_STRING)) {
            super.batchReadString(CALLER);
        } else if (cmd.equals(CMD_WRITE_INT)) {
            super.batchWriteInt(CALLER);
        } else if (cmd.equals(CMD_WRITE_STRING)) {
            super.batchWriteString(CALLER);
        }
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        System.out.println("onCreate MyService_1");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        System.out.println("onDestroy MyService_1");
    }
}
