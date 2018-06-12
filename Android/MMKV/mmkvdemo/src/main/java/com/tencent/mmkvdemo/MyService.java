package com.tencent.mmkvdemo;

import android.content.Intent;

public class MyService extends BenchMarkBaseService {
    private static final String CALLER = "MyService";

    @Override
    public void onCreate() {
        super.onCreate();
        System.out.println("onCreate MyService");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        System.out.println("onDestroy MyService");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        System.out.println("MyService onStartCommand");
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
}
