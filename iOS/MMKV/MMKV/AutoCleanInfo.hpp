/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2025 THL A29 Limited, a Tencent company.
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

#ifndef AutoCleanInfo_h
#define AutoCleanInfo_h

#include <queue>

struct AutoCleanInfo {
    NSString *m_key;
    uint64_t m_time;

    AutoCleanInfo(NSString *key, uint64_t time) {
        m_key = [key retain];
        m_time = time;
    }

    AutoCleanInfo(AutoCleanInfo &&src) {
        m_key = src.m_key;
        src.m_key = nil;
        m_time = src.m_time;
    }

    void operator=(AutoCleanInfo &&other) {
        std::swap(m_key, other.m_key);
        std::swap(m_time, other.m_time);
    }

    ~AutoCleanInfo() {
        if (m_key) {
            [m_key release];
            m_key = nil;
        }
    }

    bool operator<(const AutoCleanInfo &other) const {
        // the oldest m_time comes first
        return m_time > other.m_time;
    }

    // just forbid it for possibly misuse
    explicit AutoCleanInfo(const AutoCleanInfo &other) = delete;
    AutoCleanInfo &operator=(const AutoCleanInfo &other) = delete;
};

typedef std::priority_queue<AutoCleanInfo> AutoCleanInfoQueue_t;

#endif /* AutoCleanInfo_h */
