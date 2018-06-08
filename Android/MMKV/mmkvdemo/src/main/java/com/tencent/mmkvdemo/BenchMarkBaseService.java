package com.tencent.mmkvdemo;

import android.app.Activity;
import android.app.Service;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.support.annotation.Nullable;

import com.tencent.mmkv.MMKV;

import java.util.Random;

public abstract class BenchMarkBaseService extends Service {
    static {
        System.loadLibrary("mmkv");
    }

    public static final String CMD_ID = "cmd_id";
    public static final String CMD_READ_INT = "cmd_read_int";
    public static final String CMD_WRITE_INT = "cmd_write_int";
    public static final String CMD_READ_STRING = "cmd_read_string";
    public static final String CMD_WRITE_STRING = "cmd_write_string";

    private String[] m_arrStrings;
    private String[] m_arrKeys;
    private String[] m_arrIntKeys;

    private final int m_loops = 1000;
    private final String MMKV_ID = "benchmark_interprocess";

    @Override
    public void onCreate() {
        super.onCreate();
        System.out.println("onCreate BenchMarkBaseService");

        MMKV.initialize(this);
        {
            long startTime = System.currentTimeMillis();

            MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_THREAD_MODE);

            long endTime = System.currentTimeMillis();
            System.out.println("load [" + MMKV_ID + "]: " + (endTime - startTime) + "ms");
        }
        m_arrStrings = new String[m_loops];
        m_arrKeys = new String[m_loops];
        m_arrIntKeys = new String[m_loops];
        Random r = new Random();
        for (int index = 0; index < m_loops; index++) {
            m_arrStrings[index] = "MMKV-" + r.nextInt();
            m_arrKeys[index] = "testStr_" + index;
            m_arrIntKeys[index] = "int_" + index;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        System.out.println("onDestroy BenchMarkBaseService");

        MMKV.onExit();
//        Process.killProcess(Process.myPid());
    }

    protected void batchWriteInt(String caller) {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_THREAD_MODE);
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            mmkv.encode(key, tmp);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " batchWriteInt: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }

    protected void batchReadInt(String caller) {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_THREAD_MODE);
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = mmkv.decodeInt(key);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " batchReadInt: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }

    protected void batchWriteString(String caller) {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_THREAD_MODE);
        for (int index = 0; index < m_loops; index++) {
            final String valueStr = m_arrStrings[index];
            final String strKey = m_arrKeys[index];
            mmkv.encode(strKey, valueStr);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " batchWriteString: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }

    protected void batchReadString(String caller) {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_THREAD_MODE);
        for (int index = 0; index < m_loops; index++) {
            final String strKey = m_arrKeys[index];
            final String tmpStr = mmkv.decodeString(strKey);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " batchReadString: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }

    static class MMKVTask extends AsyncTask<ResultReceiver, Void, Void> {
        @Override
        protected Void doInBackground(ResultReceiver... params) {
            MMKV kv = MMKV.defaultMMKV(MMKV.MULTI_THREAD_MODE);
            int value = kv.decodeInt(m_key);
            System.out.println(m_key + " = " + value);
            value++;
            kv.encode(m_key, value);
//            kv.clearAll();

            ResultReceiver receiver = params[0];
            Bundle bundle = new Bundle();
            bundle.putString("value", "30");
            receiver.send(Activity.RESULT_OK, bundle);

            return null;
        }

        MMKVTask(String key) {
            m_key = key;
        }
        private String m_key;
    }

    static class KillSelf extends AsyncTask<Service, Void, Void> {
        @Override
        protected Void doInBackground(Service... services) {
            System.out.println("killing BenchMarkBaseService...");
            services[0].stopSelf();
            return null;
        }
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        System.out.println("onBind, intent=" + intent);
        return null;
    }
}
