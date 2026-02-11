/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2026 THL A29 Limited, a Tencent company.
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

package com.tencent.mmkv;

/**
 * The all-in-one configuration for creating an MMKV instance.
 */
public class MMKVConfig {
    public int mode = MMKV.SINGLE_PROCESS_MODE;

    // using AES-256 key length
    public boolean aes256 = false;

    public String cryptKey = null;

    public String rootPath = null;

    public long expectedCapacity = 0; // the initial file size

    public Boolean enableKeyExpire = null;
    public int expiredInSeconds = 0; // ExpireNever = 0

    public boolean enableCompareBeforeSet = false;

    // if not set, use the old style callback
    public MMKVRecoverStrategic recover = null;

    // the size limit of a key-value pair, reject insert if pass limit
    public int itemSizeLimit = 0;
}
