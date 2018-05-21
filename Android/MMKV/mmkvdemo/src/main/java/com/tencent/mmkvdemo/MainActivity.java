package com.tencent.mmkvdemo;

import android.content.SharedPreferences;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

import com.tencent.mmkv.MMKV;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Random;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("mmkv");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        String rootDir = MMKV.initialize(this);
        System.out.println("mmkv root: " + rootDir);

        TextView tv = (TextView) findViewById(R.id.sample_text);
        tv.setText(rootDir);


//        testMMKV();
//        testImportSharedPreferences();
//        final Handler handler = new Handler();
//        handler.postDelayed(new Runnable() {
//            @Override
//            public void run() {
//                mmkvBaselineTest(10000);
//            }
//        }, 10000);

        int loops = 1000;
        m_arrStrings = new String[loops];
        m_arrKeys = new String[loops];
        m_arrIntKeys = new String[loops];
        Random r = new Random();
        for (int index = 0; index < loops; index++) {
            m_arrStrings[index] = "MMKV-" + r.nextInt();
            m_arrKeys[index] = "testStr_" + index;
            m_arrIntKeys[index] = "int_" + index;
        }

        mmkvBaselineTest(loops);
        sharedPreferencesBaselineTest(loops);
        sqliteBaselineTest(loops);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        MMKV.onExit();
    }

    private void testMMKV() {
        MMKV kv = MMKV.defaultMMKV();

        kv.encode("bool", true);
        System.out.println("bool: " + kv.decodeBool("bool"));

        kv.encode("int", Integer.MIN_VALUE);
        System.out.println("int: " + kv.decodeInt("int"));

        kv.encode("long", Long.MAX_VALUE);
        System.out.println("long: " + kv.decodeLong("long"));

        kv.encode("float", -3.14f);
        System.out.println("float: " + kv.decodeFloat("float"));

        kv.encode("double", Double.MIN_VALUE);
        System.out.println("double: " + kv.decodeDouble("double"));

        kv.encode("string", "Hello from mmkv");
        System.out.println("string: " + kv.decodeString("string"));

        byte[] bytes = {'m', 'm', 'k', 'v'};
        kv.encode("bytes", bytes);
        System.out.println("bytes: " + new String(kv.decodeBytes("bytes")));

        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));
        System.out.println("count = " + kv.count() + ", totalSize = " + kv.totalSize());
        System.out.println("containsKey[string]: " + kv.containsKey("string"));

        kv.removeValueForKey("bool");
        System.out.println("bool: " + kv.decodeBool("bool"));
        kv.removeValuesForKeys(new String[]{"int", "long"});
//        kv.clearAll();
        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));
        System.out.println("isFileValid[" + kv.mmapID() + "]: " + MMKV.isFileValid(kv.mmapID()));
    }

    private void mmkvBaselineTest(int loops) {
        Random r = new Random();


        long startTime = System.currentTimeMillis();

        MMKV mmkv = MMKV.mmkvWithID("baseline2");
        for (int index = 0; index < loops; index++) {
//            int tmp = r.nextInt();
//            String key = m_arrIntKeys[index];
//            mmkv.encode(key, tmp);
//            tmp = mmkv.decodeInt(key);

            final String valueStr = m_arrStrings[index];
            final String strKey = m_arrKeys[index];
            mmkv.encode(strKey, valueStr);
            final String tmpStr = mmkv.decodeString(strKey);
        }
        long endTime = System.currentTimeMillis();
        System.out.println("MMKV: loop[" + loops +"]: " + (endTime-startTime) + "ms");
    }

    private void sharedPreferencesBaselineTest(int loops) {
        Random r = new Random();

        long startTime = System.currentTimeMillis();

        SharedPreferences preferences = getSharedPreferences("default2", MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        for (int index = 0; index < loops; index++) {
//            int tmp = r.nextInt();
//            String key = m_arrIntKeys[index];
//            editor.putInt(key, tmp);
//            editor.apply();
//            tmp = preferences.getInt(key, 0);

            final String str = m_arrStrings[index];
            final String key = m_arrKeys[index];
            editor.putString(key, str);
            editor.apply();
            final String tmp = preferences.getString(key, null);
        }
//        editor.commit();
        long endTime = System.currentTimeMillis();
        System.out.println("SharedPreferences: loop[" + loops +"]: " + (endTime-startTime) + "ms");
    }

    private void sqliteBaselineTest(int loops) {
        Random r = new Random();

        long startTime = System.currentTimeMillis();

        SQLIteKV sqlIteKV = new SQLIteKV(this.getApplicationContext());
//        sqlIteKV.beginTransaction();
        for (int index = 0; index < loops; index++) {
//            int tmp = r.nextInt();
//            String key = m_arrIntKeys[index];
//            sqlIteKV.putInt(key, tmp);
//            tmp = sqlIteKV.getInt(key);

            final String value = m_arrStrings[index];
            final String key = m_arrKeys[index];
            sqlIteKV.putString(key, value);
            final String tmp = sqlIteKV.getString(key);
        }
//        sqlIteKV.endTransaction();
        long endTime = System.currentTimeMillis();
        System.out.println("sqlite: loop[" + loops + "]: " + (endTime - startTime) + "ms");
    }

    private void testImportSharedPreferences() {
        SharedPreferences preferences = getSharedPreferences("imported", MODE_PRIVATE);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putBoolean("bool", true);
        editor.putInt("int", Integer.MIN_VALUE);
        editor.putLong("long", Long.MAX_VALUE);
        editor.putFloat("float", -3.14f);
        editor.putString("string", "hello, imported");
        HashSet<String> set = new HashSet<String>();
        set.add("W"); set.add("e"); set.add("C"); set.add("h"); set.add("a"); set.add("t");
        editor.putStringSet("string-set", set);
        editor.commit();

        MMKV kv = MMKV.mmkvWithID("imported");
        kv.importFromSharedPreferences(preferences);
        editor.clear().commit();

        System.out.println("allKeys: " + Arrays.toString(kv.allKeys()));

        System.out.println("bool: " + kv.getBoolean("bool", false));
        System.out.println("int: " + kv.getInt("int", 0));
        System.out.println("long: " + kv.getLong("long", 0));
        System.out.println("float: " + kv.getFloat("float", 0));
        System.out.println("double: " + kv.decodeDouble("double"));
        System.out.println("string: " + kv.getString("string", null));
        System.out.println("string-set: " + kv.getStringSet("string-set", null));
    }

    private String[] m_arrStrings;
    private String[] m_arrKeys;
    private String[] m_arrIntKeys;
}
