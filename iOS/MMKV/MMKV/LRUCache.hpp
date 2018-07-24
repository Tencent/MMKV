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

#import <list>
#import <unordered_map>

template <typename Key_t, typename Value_t>
class LRUCache {
    size_t m_capacity;
    std::list<std::pair<Key_t, Value_t>> m_list;
    std::unordered_map<Key_t, typename decltype(m_list)::iterator> m_map;

public:
    LRUCache(size_t capacity) : m_capacity(capacity) {}

    size_t size() const { return m_map.size(); }

    size_t capacity() const { return m_capacity; }

    bool contains(const Key_t &key) const { return m_map.find(key) != m_map.end(); }

    void clear() {
        m_list.clear();
        m_map.clear();
    }

    void insert(const Key_t &key, const Value_t &value) {
        auto itr = m_map.find(key);
        if (itr != m_map.end()) {
            m_list.splice(m_list.begin(), m_list, itr->second);
            itr->second->second = value;
        } else {
            if (m_map.size() == m_capacity) {
                m_map.erase(m_list.back().first);
                m_list.pop_back();
            }
            m_list.push_front(std::make_pair(key, value));
            m_map.insert(std::make_pair(key, m_list.begin()));
        }
    }

    Value_t *get(const Key_t &key) {
        auto itr = m_map.find(key);
        if (itr != m_map.end()) {
            m_list.splice(m_list.begin(), m_list, itr->second);
            return &itr->second->second;
        }
        return nullptr;
    }
};

#endif /* LRUCache_h */
