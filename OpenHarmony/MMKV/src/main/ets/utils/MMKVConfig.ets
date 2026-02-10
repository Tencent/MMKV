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
import { MMKVRecoverStrategic } from "./MMKVHandler";

/**
 * All-in-one configuration for creating a MMKV instance.
 */
export interface MMKVConfig {
  mode?: number; // Defaults to SINGLE_PROCESS_MODE
  aes256?: boolean; // using AES-256 key length
  cryptKey?: string;
  rootPath?: string;

  expectedCapacity?: bigint; // the initial file size

  enableKeyExpire?: boolean;
  expiredInSeconds?: number; // ExpireNever = 0

  enableCompareBeforeSet?: boolean;

  recover?: MMKVRecoverStrategic; // if not set, use the old style callback

  itemSizeLimit?: number; // the size limit of a key-value pair, reject insert if pass limit
}