/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
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

import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import com.tencent.mmkv.ParcelableMMKV;

public class MyService_1 extends BenchMarkBaseService implements ServiceConnection {
    public static final String CMD_PREPARE_ASHMEM = "cmd_prepare_ashmem";
    private static final String CALLER = "MyService_1";

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i("MMKV", "MyService_1 onStartCommand:");
        if (intent != null) {
            String cmd = intent.getStringExtra(CMD_ID);
            Log.i("MMKV", "----MyService_1 onStartCommand:" + cmd);
            if (cmd != null) {
                if (cmd.equals(CMD_READ_INT)) {
                    super.batchReadInt(CALLER);
                } else if (cmd.equals(CMD_READ_STRING)) {
                    super.batchReadString(CALLER);
                } else if (cmd.equals(CMD_WRITE_INT)) {
                    super.batchWriteInt(CALLER);
                } else if (cmd.equals(CMD_WRITE_STRING)) {
                    super.batchWriteString(CALLER);
                } else if (cmd.equals(CMD_PREPARE_ASHMEM)) {
                    Intent i = new Intent("com.tencent.mmkvdemo.MyService").setPackage("com.tencent.mmkvdemo");
                    bindService(i, this, BIND_AUTO_CREATE);
                } else if (cmd.equals(CMD_PREPARE_ASHMEM_BY_CP)) {
                    super.prepareAshmemMMKVByCP();
                }
            }
        }
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i("MMKV", "onCreate MyService_1");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i("MMKV", "onDestroy MyService_1");
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        IAshmemMMKV ashmemMMKV = IAshmemMMKV.Stub.asInterface(service);
        try {
            ParcelableMMKV parcelableMMKV = ashmemMMKV.GetAshmemMMKV();
            if (parcelableMMKV != null) {
                m_ashmemMMKV = parcelableMMKV.toMMKV();
                if (m_ashmemMMKV != null) {
                    Log.i("MMKV", "ashmem bool: " + m_ashmemMMKV.decodeBool("bool"));
                }
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
    }
}
