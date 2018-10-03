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

#import "CityHash.h"
#import "DynamicBitset.hpp"
#import "MMBuffer.h"
#import "tsl/hopscotch_map.h"
#import <Foundation/Foundation.h>
#import <memory>
#import <vector>

#pragma pack(push, 1)
struct KeyValueHolder {
    uint32_t offset;
    uint32_t keySize : 10;
    uint32_t valueSize : 22;

    inline size_t end() const { return offset + keySize + valueSize; }

    inline bool operator<(const KeyValueHolder &other) const { return (offset < other.offset); }
};
#pragma pack(pop)

typedef MMBuffer KeyHolder;
struct KeyHolderHashFunctor {
    size_t operator()(const KeyHolder &key) const {
        return static_cast<size_t>(CityHash64((const char *) key.getPtr(), key.length()));
    }
};

struct KeyHolderEqualFunctor {
    bool operator()(const KeyHolder &left, const KeyHolder &right) const {
        auto leftLength = left.length(), rightLength = right.length();
        if (leftLength != rightLength) {
            return false;
        }
        return (memcmp(left.getPtr(), right.getPtr(), leftLength) == 0);
    }
};

class KVItemsWrap {
    DynamicBitset m_deleteMark;
    std::vector<KeyValueHolder> m_vector;
    tsl::hopscotch_map<KeyHolder,
                       size_t,
                       KeyHolderHashFunctor,
                       std::equal_to<>,
                       std::allocator<std::pair<KeyHolder, size_t>> /*, 30, true*/>
        m_dictionary;

public:
    KVItemsWrap() {}

    void emplace(KeyHolder &&key, KeyValueHolder &&value);

    void erase(const KeyHolder &key);
    void erase(NSString *__unsafe_unretained nsKey);

    KeyValueHolder *find(const KeyHolder &key);
    KeyValueHolder *find(NSString *__unsafe_unretained nsKey);

    inline void clear() {
        m_vector.clear();
        m_dictionary.clear();
        m_deleteMark.reset();
    }

    inline KeyValueHolder &operator[](size_t index) { return m_vector[index]; }

    inline size_t size() const { return m_dictionary.size(); }

    std::vector<std::pair<size_t, size_t>> mergeNearbyItems();

private:
    // just forbid it for possibly misuse
    KVItemsWrap(const KVItemsWrap &other) = delete;
    KVItemsWrap &operator=(const KVItemsWrap &other) = delete;
};
