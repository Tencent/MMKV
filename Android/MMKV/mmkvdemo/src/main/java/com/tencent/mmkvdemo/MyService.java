package com.tencent.mmkvdemo;

import android.app.Activity;
import android.app.Service;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.support.annotation.Nullable;
import android.os.Process;
import com.tencent.mmkv.MMKV;

public class MyService extends Service {
    static {
        System.loadLibrary("mmkv");
    }

    public static String BUNDLED_LISTENER = "MyServiceListener";
    public static String KEY_NAME = "key_name";

    public MyService() {
    }

    @Override
    public void onCreate() {
        super.onCreate();
        System.out.println("onCreate MyService");

        MMKV.initialize(this);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        System.out.println("onDestroy MyService");

        Process.killProcess(Process.myPid());
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        System.out.println("onBind, intent=" + intent);
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        System.out.println("onStartCommand");

        ResultReceiver receiver = intent.getParcelableExtra(MyService.BUNDLED_LISTENER);
        String key = intent.getStringExtra(MyService.KEY_NAME);
        new MMKVTask(key).execute(receiver);

//        new KillSelf().execute(this);

        return super.onStartCommand(intent, flags, startId);
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
            System.out.println("killing MyService...");
            services[0].stopSelf();
            return null;
        }
    }
}
