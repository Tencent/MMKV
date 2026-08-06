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

#include "MMKVStringCodec.h"
#include <cassert>
#include <string>
#include <vector>

using namespace std;
using namespace mmkv;

int main() {
    string canonical = "before";
    canonical.push_back('\0');
    canonical.append("\xF0\x9F\x98\x80", 4);
    canonical.append("after");

    string modified = "before";
    modified.append("\xC0\x80", 2);
    modified.append("\xED\xA0\xBD\xED\xB8\x80", 6);
    modified.append("after");

    const vector<uint16_t> expected = {
        'b', 'e', 'f', 'o', 'r', 'e', 0, 0xD83D, 0xDE00, 'a', 'f', 't', 'e', 'r',
    };
    vector<uint16_t> decoded;
    assert(android::decodeUtf8OrModifiedUtf8(canonical, decoded));
    assert(decoded == expected);
    assert(android::decodeUtf8OrModifiedUtf8(modified, decoded));
    assert(decoded == expected);

    string mixed("\0", 1);
    mixed.append("\xED\xA0\xBD\xED\xB8\x80", 6);
    const vector<string> malformed = {
        string("\xFF", 1),
        string("\xC0\xAF", 2),
        string("\xED\xA0", 2),
        string("\xF0\x80\x80\x80", 4),
        mixed,
    };
    for (const auto &value : malformed) {
        decoded = {1};
        assert(!android::decodeUtf8OrModifiedUtf8(value, decoded));
        assert(decoded.empty());
    }
}
