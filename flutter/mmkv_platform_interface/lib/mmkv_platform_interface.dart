/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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

abstract base class MMKVPluginPlatform {
  MMKVPluginPlatform();

  // A plugin can have a default implementation, as shown here, or `instance`
  // can be nullable, and the default instance can be null.
  static MMKVPluginPlatform instance = MMKVPluginDefault();

// Methods for the plugin's platform interface would go here, often with
// implementations that throw UnimplementedError.
}

final class MMKVPluginDefault extends MMKVPluginPlatform {
  // A default real implementation of the platform interface would go here.
}