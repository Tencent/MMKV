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
 *	   https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import <algorithm>
#import <vector>

class DynamicBitset {
    size_t m_size;
    std::vector<size_t> m_vector;
    static constexpr unsigned int CellBitSize = 8 * sizeof(size_t);

    void ensureSize(size_t index) {
        if (index >= m_size) {
            do {
                m_size *= 2;
            } while (index >= m_size);
            resize(m_size);
        }
    }

public:
    DynamicBitset(size_t size = 1024) { resize(size); }

    size_t size() const { return m_size; }

    void resize(size_t size) {
        m_size = size;
        m_vector.resize((size + CellBitSize - 1) / CellBitSize);
    }

    void reset() { std::fill(m_vector.begin(), m_vector.end(), 0); }

    void reset(size_t index) {
        m_vector[index / CellBitSize] &= ~(size_t(1) << (index % CellBitSize));
    }

    void set(size_t index) {
        ensureSize(index);
        m_vector[index / CellBitSize] |= size_t(1) << (index % CellBitSize);
    }

    bool test(size_t index) const {
        if (index >= m_size) {
            return false;
        }
        return (m_vector[index / CellBitSize] & (size_t(1) << (index % CellBitSize))) != 0;
    }

    bool any() const {
        for (auto cell : m_vector) {
            if (cell != 0) {
                return true;
            }
        }
        return false;
    }

    size_t popCount(size_t index) const {
        size_t count = 0;
        for (size_t index = 0, end = (std::min(m_size, index) + CellBitSize - 1) / CellBitSize; index < end;
             index++) {
            count += __builtin_popcountll(m_vector[index]);
        }
        return count;
    }

private:
    bool &operator[](size_t index) = delete;
};
