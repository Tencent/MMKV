package com.tencent.mmkvdemo;

import android.app.Service;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.IBinder;
import android.support.annotation.Nullable;

import java.util.Random;

import com.tencent.mmkv.MMKV;

public abstract class BenchMarkBaseService extends Service {
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
    private final String SP_ID = "benchmark_interprocess_sp";

    @Override
    public void onCreate() {
        super.onCreate();
        System.out.println("onCreate BenchMarkBaseService");

        MMKV.initialize(this);
        {
            long startTime = System.currentTimeMillis();

            MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_PROCESS_MODE);

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
        mmkvBatchWriteInt(caller);
        sqliteWriteInt(caller);
        spBatchWriteInt(caller);
    }
    private void mmkvBatchWriteInt(String caller) {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_PROCESS_MODE);
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            mmkv.encode(key, tmp);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " mmkv write int: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void sqliteWriteInt(String caller) {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(this);
//        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            sqlIteKV.putInt(key, tmp);
        }
//        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " sqlite write int: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void spBatchWriteInt(String caller) {
        Random r = new Random();
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            int tmp = r.nextInt();
            String key = m_arrIntKeys[index];
            editor.putInt(key, tmp);
//            editor.commit();
            editor.apply();
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " MultiProcessSharedPreferences write int: loop[" + m_loops +"]: " + (endTime-startTime) + "ms");
    }

    protected void batchReadInt(String caller) {
        mmkvBatchReadInt(caller);
        sqliteReadInt(caller);
        spBatchReadInt(caller);
    }
    private void mmkvBatchReadInt(String caller) {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_PROCESS_MODE);
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = mmkv.decodeInt(key);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " mmkv read int: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void sqliteReadInt(String caller) {
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(this);
//        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = sqlIteKV.getInt(key);
        }
//        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " sqlite read int: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void spBatchReadInt(String caller) {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            String key = m_arrIntKeys[index];
            int tmp = preferences.getInt(key, 0);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " MultiProcessSharedPreferences read int: loop[" + m_loops +"]: " + (endTime-startTime) + "ms");
    }

    protected void batchWriteString(String caller) {
        mmkvBatchWriteString(caller);
        sqliteWriteString(caller);
        spBatchWrieString(caller);
    }
    private void mmkvBatchWriteString(String caller) {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_PROCESS_MODE);
        for (int index = 0; index < m_loops; index++) {
            final String valueStr = m_arrStrings[index];
            final String strKey = m_arrKeys[index];
            mmkv.encode(strKey, valueStr);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " mmkv write String: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void sqliteWriteString(String caller) {
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(this);
//        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            final String value = m_arrStrings[index];
            final String key = m_arrKeys[index];
            sqlIteKV.putString(key, value);
        }
//        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " sqlite write String: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void spBatchWrieString(String caller) {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < m_loops; index++) {
            final String str = m_arrStrings[index];
            final String key = m_arrKeys[index];
            editor.putString(key, str);
//            editor.commit();
            editor.apply();
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " MultiProcessSharedPreferences write String: loop[" + m_loops +"]: " + (endTime-startTime) + "ms");
    }

    protected void batchReadString(String caller) {
        mmkvBatchReadString(caller);
        sqliteReadString(caller);
        spBatchReadStrinfg(caller);
    }
    private void mmkvBatchReadString(String caller) {
        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID(MMKV_ID, MMKV.MULTI_PROCESS_MODE);
        for (int index = 0; index < m_loops; index++) {
            final String strKey = m_arrKeys[index];
            final String tmpStr = mmkv.decodeString(strKey);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " mmkv read String: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void sqliteReadString(String caller) {
        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(this);
//        sqlIteKV.beginTransaction();
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = sqlIteKV.getString(key);
        }
//        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " sqlite read String: loop[" + m_loops + "]: " + (endTime - startTime) + "ms");
    }
    private void spBatchReadStrinfg(String caller) {
        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = MultiProcessSharedPreferences.getSharedPreferences(this, SP_ID, MODE_PRIVATE);
        for (int index = 0; index < m_loops; index++) {
            final String key = m_arrKeys[index];
            final String tmp = preferences.getString(key, null);
        }
        long endTime = System.currentTimeMillis();
        System.out.println(caller + " MultiProcessSharedPreferences read String: loop[" + m_loops +"]: " + (endTime-startTime) + "ms");
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        System.out.println("onBind, intent=" + intent);
        return null;
    }
}
