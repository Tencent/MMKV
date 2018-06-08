package com.tencent.mmkvdemo;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.ResultReceiver;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
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

        String rootDir = "mmkv root: " + MMKV.initialize(this);
        System.out.println(rootDir);

        TextView tv = (TextView) findViewById(R.id.sample_text);
        tv.setText(rootDir);

        final Button button = findViewById(R.id.button);
        button.setOnClickListener(new View.OnClickListener() {
            final Baseline baseline = new Baseline(getApplicationContext(), 1000);

            public void onClick(View v) {
                baseline.mmkvBaselineTest();
                baseline.sharedPreferencesBaselineTest();
                baseline.sqliteBaselineTest();
            }
        });

        final Button button_read_int = findViewById(R.id.button_read_int);
        button_read_int.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_READ_INT);
            }
        });

        final Button button_write_int = findViewById(R.id.button_write_int);
        button_write_int.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_WRITE_INT);
            }
        });

        final Button button_read_string = findViewById(R.id.button_read_string);
        button_read_string.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_READ_STRING);
            }
        });

        final Button button_write_string = findViewById(R.id.button_write_string);
        button_write_string.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                interProcessBaselineTest(BenchMarkBaseService.CMD_WRITE_STRING);
            }
        });

        testMMKV();
//        testInterProcessLock();
//        testImportSharedPreferences();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        MMKV.onExit();
    }

    private void testInterProcessLock() {
        Intent intent = new Intent(this, MyService.class);
        startService(intent);
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

    private void interProcessBaselineTest(String cmd) {
        Intent intent = new Intent(this, MyService.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, cmd);
        startService(intent);

        intent = new Intent(this, MyService_1.class);
        intent.putExtra(BenchMarkBaseService.CMD_ID, cmd);
        startService(intent);
    }
}
