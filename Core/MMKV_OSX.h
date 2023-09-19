/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
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

#pragma once
#include "MMKVPredef.h"

MMKV_NAMESPACE_BEGIN

#if defined(MMKV_IOS) && defined(__cplusplus)

class MLockPtr {
    size_t m_lockDownSize;
    uint8_t *m_lockedPtr;

public:
    MLockPtr(void *ptr, size_t size);
    MLockPtr(MLockPtr &&other);

    ~MLockPtr();

    bool isLocked() const {
        return (m_lockedPtr != nullptr);
    }

    static bool isMLockPtrEnabled;

    // just forbid it for possibly misuse
    explicit MLockPtr(const MLockPtr &other) = delete;
    MLockPtr &operator=(const MLockPtr &other) = delete;
};

std::pair<bool, MLockPtr> guardForBackgroundWriting(void *ptr, size_t size);

#endif

enum { UnKnown = 0, PowerMac = 1, Mac, iPhone, iPod, iPad, AppleTV, AppleWatch };

void GetAppleMachineInfo(int &device, int &version);

MMKV_NAMESPACE_END
