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

#import "KeyValueHolder.h"

#if __has_feature(objc_arc)
#error This file must be compiled with MRC. Use -fno-objc-arc flag.
#endif

using namespace std;

void KVItemsWrap::emplace(KeyHolder &&key, KeyValueHolder &&value) {
	size_t index = m_vector.size();
	m_vector.emplace_back(value);
	auto ret = m_dictionary.try_emplace(std::move(key), index);
	if (!ret.second) {
		auto oldValue = ret.first.value();
		m_deleteMark.set(oldValue);
		ret.first.value() = index;
	}
	//assert(m_vector.size() != m_dictionary.size() + m_deleteMark.popCount(m_vector.size()));
}

void KVItemsWrap::erase(const KeyHolder &key) {
	auto itr = m_dictionary.find(key);
	if (itr != m_dictionary.end()) {
		auto oldValue = itr.value();
		m_deleteMark.set(oldValue);
		m_dictionary.erase(itr);
	}
	//assert(m_vector.size() == m_dictionary.size() + m_deleteMark.popCount(m_vector.size()));
}

void KVItemsWrap::erase(NSString *__unsafe_unretained nsKey) {
	const char *ch = nsKey.UTF8String;
	if (!ch) {
		return;
	}
	size_t length = strlen(ch);
	KeyHolder key((void *) ch, length, MMBufferNoCopy);

	this->erase(key);
}

KeyValueHolder *KVItemsWrap::find(const KeyHolder &key) {
	auto itr = m_dictionary.find(key);
	if (itr != m_dictionary.end()) {
		auto index = itr.value();
		return &m_vector[index];
	}
	return nullptr;
}

KeyValueHolder *KVItemsWrap::find(NSString *__unsafe_unretained nsKey) {
	const char *ch = nsKey.UTF8String;
	if (!ch) {
		return nullptr;
	}
	size_t length = strlen(ch);
	KeyHolder key((void *) ch, length, MMBufferNoCopy);

	return this->find(key);
}

vector<pair<size_t, size_t>> KVItemsWrap::mergeNearbyItems() {
	if (m_deleteMark.any()) {
		vector<int32_t> moveDiff(m_vector.size(), 0);
		size_t emptyPos = 0, index = 1;
		for (size_t end = m_vector.size(); index < end; emptyPos++, index++) {
			// find first empty position
			while (emptyPos < end && !m_deleteMark.test(emptyPos)) {
				emptyPos++;
			}
			if (emptyPos >= end) {
				break;
			}
			// find first in-use position
			index = std::max(index, emptyPos + 1);
			while (index < end && m_deleteMark.test(index)) {
				index++;
			}
			if (index >= end) {
				break;
			}
			// move
			m_vector[emptyPos] = m_vector[index];
			m_deleteMark.reset(emptyPos);
			m_deleteMark.set(index);
			moveDiff[index] = static_cast<int32_t>(emptyPos - index);
		}

		// delete [emptyPos, end)
		//		assert(m_vector.size() == emptyPos + m_deleteMark.popCount(m_vector.size()));
		if (emptyPos < m_vector.size()) {
			m_vector.erase(m_vector.begin() + emptyPos, m_vector.end());
		}
		m_deleteMark.reset();

		for (auto itr = m_dictionary.begin(), end = m_dictionary.end(); itr != end; itr++) {
			auto &value = itr.value();
			auto diff = moveDiff[value];
			if (diff) {
				value += diff;
			}
		}
	}
	assert(m_dictionary.size() == m_vector.size());

	vector<pair<size_t, size_t>> arrMergedItems;
	if (!m_vector.empty()) {
		arrMergedItems.emplace_back(0, 0);
		pair<size_t, size_t> *lastSegment = &arrMergedItems.back();
		size_t lastHolderEnd = m_vector[0].end();
		for (size_t index = 1, end = m_vector.size(); index < end; index++) {
			const auto &kvHolder = m_vector[index];
			if (lastHolderEnd != kvHolder.offset) {
				arrMergedItems.emplace_back(index, index);
				lastSegment = &arrMergedItems.back();
			} else {
				lastSegment->second = index;
			}
			lastHolderEnd = kvHolder.end();
		}
	}
	return arrMergedItems;
}
