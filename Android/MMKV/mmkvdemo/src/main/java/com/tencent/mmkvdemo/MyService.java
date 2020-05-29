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

import android.content.Intent;
import android.util.Log;
import com.tencent.mmkv.MMKV;

public class MyService extends BenchMarkBaseService {
    private static final String CALLER = "MyService";
    public static final String CMD_REMOVE = "cmd_remove";

    public static final String CMD_LOCK = "cmd_lock";
    public static final String LOCK_PHASE_1 = "lock_phase_1";
    public static final String LOCK_PHASE_2 = "lock_phase_2";
    public static final String CMD_TRIM_FINISH = "trim_finish";

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i("MMKV", "onCreate MyService");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i("MMKV", "onDestroy MyService");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i("MMKV", "MyService onStartCommand");
        if (intent != null) {
            String cmd = intent.getStringExtra(CMD_ID);
            if (cmd != null) {
                if (cmd.equals(CMD_READ_INT)) {
                    super.batchReadInt(CALLER);
                } else if (cmd.equals(CMD_READ_STRING)) {
                    super.batchReadString(CALLER);
                } else if (cmd.equals(CMD_WRITE_INT)) {
                    super.batchWriteInt(CALLER);
                } else if (cmd.equals(CMD_WRITE_STRING)) {
                    super.batchWriteString(CALLER);
                } else if (cmd.equals(CMD_PREPARE_ASHMEM_BY_CP)) {
                    super.prepareAshmemMMKVByCP();
                } else if (cmd.equals(CMD_REMOVE)) {
                    testRemove();
                } else if (cmd.equals(CMD_LOCK)) {
                    testLock();
                } else if (cmd.equals(CMD_TRIM_FINISH)) {
                    testTrimNonEmpty();
                }
            }
        }
        return super.onStartCommand(intent, flags, startId);
    }

    private void testRemove() {
        MMKV mmkv = GetMMKV();
        Log.d("mmkv in child", "" + mmkv.decodeInt(CMD_ID));
        mmkv.remove(CMD_ID);
    }

    private void testLock() {
        // get the lock immediately
        MMKV mmkv2 = MMKV.mmkvWithID(LOCK_PHASE_2, MMKV.MULTI_PROCESS_MODE);
        mmkv2.lock();
        Log.d("locked in child", LOCK_PHASE_2);

        Runnable waiter = new Runnable() {
            @Override
            public void run() {
                MMKV mmkv1 = MMKV.mmkvWithID(LOCK_PHASE_1, MMKV.MULTI_PROCESS_MODE);
                mmkv1.lock();
                Log.d("locked in child", LOCK_PHASE_1);
            }
        };
        // wait infinitely
        new Thread(waiter).start();
    }

    private void testTrimNonEmpty() {
        MMKV mmkv = MMKV.mmkvWithID("trimNonEmptyInterProcess", MMKV.MULTI_PROCESS_MODE);
        byte[] value = new byte[64];
        mmkv.encode("SomeKey", value);
    }
}
