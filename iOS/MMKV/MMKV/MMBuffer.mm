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

#import "MMBuffer.h"
#import <cstring>
#import <utility>

MMBuffer::MMBuffer(size_t length)
    : ptr(nullptr), size(length), isNoCopy(MMBufferCopy) {
	if (size > 0) {
		if (!canUseSmallBuffer()) {
			ptr = malloc(size);
			if (!ptr) {
				size = 0;
			}
		}
	}
}

MMBuffer::MMBuffer(void *source, size_t length, MMBufferCopyFlag noCopy)
    : ptr(source), size(length), isNoCopy(noCopy) {
	if (isNoCopy == MMBufferCopy) {
		if (canUseSmallBuffer()) {
			memcpy(smallBuffer, source, size);
			ptr = nullptr;
		} else {
			ptr = malloc(size);
			if (ptr) {
				memcpy(ptr, source, size);
			} else {
				size = 0;
			}
		}
	}
}

MMBuffer::MMBuffer(MMBuffer &&other) noexcept
    : ptr(other.ptr), size(other.size), isNoCopy(other.isNoCopy) {
	if (iSUsingSmallBuffer()) {
		memcpy(smallBuffer, other.smallBuffer, size);
	}
	other.ptr = nullptr;
	other.size = 0;
	other.isNoCopy = MMBufferCopy;
}

void MMBuffer::swap(MMBuffer &&other) {
	if (this == &other) {
		return;
	}
	if (iSUsingSmallBuffer() || other.iSUsingSmallBuffer()) {
		char tmp[SmallBufferSize];
		memcpy(tmp, smallBuffer, SmallBufferSize);
		memcpy(smallBuffer, other.smallBuffer, SmallBufferSize);
		memcpy(other.smallBuffer, tmp, SmallBufferSize);
	}
	std::swap(ptr, other.ptr);
	std::swap(size, other.size);
	std::swap(isNoCopy, other.isNoCopy);
}

MMBuffer::~MMBuffer() {
	if (isNoCopy == MMBufferCopy && ptr) {
		free(ptr);
	}
}

bool MMBuffer::operator==(const MMBuffer &other) const {
	if (size != other.size) {
		return false;
	}
	if (ptr) {
		if (other.ptr) {
			return (memcmp(ptr, other.ptr, size) == 0);
		} else {
			return (memcmp(ptr, other.smallBuffer, size) == 0);
		}
	} else {
		if (other.ptr) {
			return (memcmp(smallBuffer, other.ptr, size) == 0);
		} else {
			return (memcmp(smallBuffer, other.smallBuffer, size) == 0);
		}
	}
}
