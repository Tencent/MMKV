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

#ifndef LRUCache_h
#define LRUCache_h

#import <functional>
#import <list>
#import <unordered_map>

// a generic LRU-Cache
// if the amount of items is huge, replace std::unordered_map with tsl::hopscotch_map (currently not necessary inside MMKV)
template <typename Key_t, typename Value_t>
class LRUCache {
    size_t m_capacity;
    std::list<std::pair<Key_t, Value_t>> m_list;
    typedef typename decltype(m_list)::iterator ItrType;
    std::unordered_map<Key_t, ItrType> m_map;
    Key_t m_lastKey;
    Value_t *m_lastValue;

public:
    LRUCache(size_t capacity)
        : m_capacity(capacity), m_lastKey(std::numeric_limits<Key_t>::max()), m_lastValue(nullptr) {
        m_map.reserve(capacity * 2);
    }

    size_t size() const { return m_map.size(); }

    size_t capacity() const { return m_capacity; }

    void clear() {
        m_list.clear();
        m_map.clear();
        m_lastKey = std::numeric_limits<Key_t>::max();
        m_lastValue = nullptr;
    }

    void insert(const Key_t &key, const Value_t &value) {
        auto itr = m_map.find(key);
        if (itr != m_map.end()) {
            m_list.splice(m_list.begin(), m_list, itr->second);
            itr->second->second = value;
            m_lastKey = key;
            m_lastValue = &itr->second->second;
        } else {
            if (m_map.size() == m_capacity) {
                m_map.erase(m_list.back().first);
                m_list.pop_back();
            }
            m_list.push_front(std::make_pair(key, value));
            m_map.insert(std::make_pair(key, m_list.begin()));
            m_lastKey = key;
            m_lastValue = &m_list.front().second;
        }
    }

    Value_t *get(const Key_t &key) {
        if (key == m_lastKey) {
            return m_lastValue;
        }
        auto itr = m_map.find(key);
        if (itr != m_map.end()) {
            m_list.splice(m_list.begin(), m_list, itr->second);
            m_lastKey = key;
            m_lastValue = &itr->second->second;
            return m_lastValue;
        }
        return nullptr;
    }

    void forEach(std::function<void(Value_t &)> function) {
        for (auto itr = m_list.begin(), end = m_list.end(); itr != end; itr++) {
            function(itr->second);
        }
    }
};

#endif /* LRUCache_h */
