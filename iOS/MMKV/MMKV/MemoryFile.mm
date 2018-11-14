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

#import "MemoryFile.h"
#import "MMKVLog.h"
#import <errno.h>
#import <fcntl.h>
#import <libgen.h>
#import <sys/mman.h>
#import <sys/stat.h>
#import <sys/types.h>
#import <unistd.h>

using namespace std;

const int DEFAULT_MMAP_SIZE = getpagesize();
// 1MB per segment
constexpr uint32_t SegmentSize = 1024 * 1024;
// count of segments in memory
constexpr size_t SegmentCapacity = 10;

MemoryFile::MemoryFile(NSString *path)
    : m_name(path), m_fd(-1), m_ptr(nullptr), m_size(0), m_segmentCache(SegmentCapacity) {
	m_fd = open(m_name.UTF8String, O_RDWR | O_CREAT, S_IRWXU);
	if (m_fd < 0) {
		MMKVError(@"fail to open:%@, %s", m_name, strerror(errno));
	} else {
		struct stat st = {};
		if (fstat(m_fd, &st) != -1) {
			m_size = static_cast<size_t>(st.st_size);
		}
		// round up to (n * pagesize)
		if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
			size_t roundSize = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
			ftruncate(roundSize);
		} else {
			// pre-load
			prepareSegments();
		}
	}
}

MemoryFile::~MemoryFile() {
	if (m_fd >= 0) {
		close(m_fd);
		m_fd = -1;
	}
}

bool MemoryFile::ftruncate(size_t size) {
	if (size == m_size) {
		return true;
	}
	size_t oldSize = m_size;
	m_size = size;
	// round up to (n * pagesize)
	if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
		m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
	}
	if (::ftruncate(m_fd, m_size) != 0) {
		MMKVError(@"fail to truncate [%@] to size %zu, %s", m_name, m_size, strerror(errno));
		m_size = oldSize;
		return false;
	}
	if (m_size > oldSize) {
		zeroFillFile(m_fd, oldSize, m_size - oldSize);
	}
	m_segmentCache.clear();
	prepareSegments();
	return true;
}

void MemoryFile::prepareSegments() {
	auto end = (m_size + SegmentSize - 1) / SegmentSize;
	end = min(end, m_segmentCache.capacity());
	for (uint32_t index = 0; index < end; index++) {
		tryGetSegment(index);
	}
	// I'm felling lucky
	if (m_size <= SegmentSize && m_segmentCache.size() == 1) {
		m_ptr = *m_segmentCache.get(0);
	}
}

shared_ptr<MemoryFile::Segment> MemoryFile::tryGetSegment(uint32_t index) {
	auto segmentPtr = m_segmentCache.get(index);
	if (segmentPtr) {
		return *segmentPtr;
	} else {
		// mmap segment from file
		size_t offset = index * SegmentSize;
		size_t size = min<size_t>(SegmentSize, m_size - offset);
		void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, offset);
		if (ptr != MAP_FAILED) {
			auto segment = shared_ptr<Segment>(new Segment(ptr, size, offset));
			m_segmentCache.insert(index, segment);
			return segment;
		} else {
			MMKVError(@"fail to mmap %@ with size %zu at position %zu, %s", m_name, size, offset, strerror(errno));
		}
	}
	return nullptr;
}

uint32_t MemoryFile::offset2index(size_t offset) const {
	return static_cast<uint32_t>(offset / SegmentSize);
}

NSData *MemoryFile::read(size_t offset, size_t size) {
	if (offset >= m_size || m_size - offset < size || size == 0) {
		return nil;
	}
	// I'm felling lucky
	if (m_ptr) {
		return [NSData dataWithBytesNoCopy:m_ptr->ptr + offset length:size];
	}
	// most of the case, just return a shadow without copying any data
	auto index = offset2index(offset);
	auto segment = tryGetSegment(index);
	if (!segment) {
		return nil;
	}
	if (offset + size <= segment->offset + segment->size) {
		auto ptr = segment->ptr + (offset - segment->offset);
		return [NSData dataWithBytesNoCopy:ptr length:size];
	}
	// one segment is not enough, we have to copy data crossing segments
	auto endIndex = offset2index(offset + size);
	NSMutableData *data = [NSMutableData data];
	for (auto inc = index; inc <= endIndex; inc++) {
		auto segment = tryGetSegment(inc);
		if (!segment) {
			return nil;
		}
		auto ptr = segment->ptr;
		size_t copySize = 0;
		if (offset >= segment->offset) {
			// it's the begin
			ptr += offset - segment->offset;
			copySize = segment->offset + segment->size - offset;
		} else if (offset < segment->offset && (offset + size) > (segment->offset + segment->size)) {
			// it's the middle(s)
			copySize = segment->size;
		} else {
			// it's the end
			copySize = offset + size - segment->offset;
		}
		[data appendBytes:ptr length:copySize];
	}
	return nil;
}

bool MemoryFile::write(size_t offset, const void *source, size_t size) {
	if (offset >= m_size || m_size - offset < size || source == nullptr || size == 0) {
		return false;
	}
	// I'm felling lucky
	if (m_ptr) {
		memcpy(m_ptr->ptr + offset, source, size);
		return true;
	}
	// most of the case, just write to one segment
	auto index = offset2index(offset);
	auto segment = tryGetSegment(index);
	if (!segment) {
		return false;
	} else if (offset + size <= segment->offset + segment->size) {
		auto ptr = segment->ptr + (offset - segment->offset);
		memcpy(ptr, source, size);
		return true;
	}
	// one segment is not enough, we have to write data crossing segments
	auto endIndex = offset2index(offset + size);
	for (auto inc = index; inc <= endIndex; inc++) {
		auto segment = tryGetSegment(inc);
		if (!segment) {
			return false;
		}
		auto ptr = segment->ptr;
		size_t writeSize = 0;
		if (offset >= segment->offset) {
			// it's the begin
			ptr += offset - segment->offset;
			writeSize = segment->offset + segment->size - offset;
		} else if (offset < segment->offset && (offset + size) > (segment->offset + segment->size)) {
			// it's the middle(s)
			writeSize = segment->size;
		} else {
			// it's the end
			writeSize = offset + size - segment->offset;
		}
		memcpy(ptr, source, writeSize);
		source = (uint8_t *) source + writeSize;
	}
	return true;
}

bool MemoryFile::innerMemcpy(size_t targetOffset, size_t sourceOffset, size_t size) {
	if (targetOffset >= m_size || sourceOffset >= m_size || m_size - targetOffset < size || m_size - sourceOffset < size) {
		return false;
	}
	if (size == 0) {
		return true;
	}
	// I'm felling lucky
	if (m_ptr) {
		memcpy(m_ptr->ptr + targetOffset, m_ptr->ptr + sourceOffset, size);
		return true;
	}
	do {
		auto targetIndex = offset2index(targetOffset);
		auto sourceIndex = offset2index(sourceOffset);
		auto targetSegment = tryGetSegment(targetIndex);
		auto sourceSegment = tryGetSegment(sourceIndex);
		if (!targetSegment || !sourceSegment) {
			return false;
		}
		auto targetPtr = targetSegment->ptr + (targetOffset - targetSegment->offset);
		size_t targetSize = targetSegment->offset + targetSegment->size - targetOffset;

		auto sourcePtr = sourceSegment->ptr + (sourceOffset - sourceSegment->offset);
		size_t sourceSize = sourceSegment->offset + sourceSegment->size - sourceOffset;

		size_t copySize = min({targetSize, sourceSize, size});
		memcpy(targetPtr, sourcePtr, copySize);

		size -= copySize;
		targetOffset -= copySize;
		sourceOffset -= copySize;
	} while (size > 0);

	return true;
}

#pragma mark - Segment

MemoryFile::Segment::Segment(void *source, size_t _size, size_t _offset) noexcept
    : ptr(static_cast<uint8_t *>(source)), size(_size), offset(_offset) {
	assert(ptr);
}

MemoryFile::Segment::Segment(Segment &&other) noexcept
    : ptr(other.ptr), size(other.size), offset(other.offset) {
	other.ptr = nullptr;
	other.size = 0;
	other.offset = 0;
}

MemoryFile::Segment::~Segment() {
	if (ptr) {
		munmap(ptr, size);
		ptr = nullptr;
	}
}

#pragma mark - file

bool isFileExist(NSString *nsFilePath) {
	if (nsFilePath.length == 0) {
		return false;
	}

	struct stat temp;
	return lstat(nsFilePath.UTF8String, &temp) == 0;
}

bool createFile(NSString *nsFilePath) {
	NSFileManager *oFileMgr = [NSFileManager defaultManager];
	// try create file at once
	NSMutableDictionary *fileAttr = [NSMutableDictionary dictionary];
#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
	[fileAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication
	             forKey:NSFileProtectionKey];
#endif
	if ([oFileMgr createFileAtPath:nsFilePath contents:nil attributes:fileAttr]) {
		return true;
	}

	// create parent directories
	NSString *nsPath = [nsFilePath stringByDeletingLastPathComponent];

	//path is not nullptr && is not '/'
	NSError *err;
	if ([nsPath length] > 1 && ![oFileMgr createDirectoryAtPath:nsPath withIntermediateDirectories:YES attributes:nil error:&err]) {
		MMKVError(@"create file path:%@ fail:%@", nsPath, [err localizedDescription]);
		return false;
	}
	// create file again
	if (![oFileMgr createFileAtPath:nsFilePath contents:nil attributes:fileAttr]) {
		MMKVError(@"create file path:%@ fail.", nsFilePath);
		return false;
	}
	return true;
}

void tryResetFileProtection(NSString *path) {
#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
	@autoreleasepool {
		NSDictionary *attr = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:nullptr];
		NSString *protection = [attr valueForKey:NSFileProtectionKey];
		MMKVInfo(@"protection on [%@] is %@", path, protection);
		if ([protection isEqualToString:NSFileProtectionCompleteUntilFirstUserAuthentication] == NO) {
			NSMutableDictionary *newAttr = [NSMutableDictionary dictionaryWithDictionary:attr];
			[newAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication forKey:NSFileProtectionKey];
			NSError *err = nil;
			[[NSFileManager defaultManager] setAttributes:newAttr ofItemAtPath:path error:&err];
			if (err != nil) {
				MMKVError(@"fail to set attribute %@ on [%@]: %@", NSFileProtectionCompleteUntilFirstUserAuthentication, path, err);
			}
		}
	}
#endif
}

bool removeFile(NSString *nsFilePath) {
	int ret = unlink(nsFilePath.UTF8String);
	if (ret != 0) {
		MMKVError(@"remove file failed. filePath=%@, err=%s", nsFilePath, strerror(errno));
		return false;
	}
	return true;
}

bool zeroFillFile(int fd, size_t startPos, size_t size) {
	if (fd < 0) {
		return false;
	}

	if (lseek(fd, startPos, SEEK_SET) < 0) {
		MMKVError(@"fail to lseek fd[%d], error:%s", fd, strerror(errno));
		return false;
	}

	static const char zeros[4 * 1024] = {0};
	while (size >= sizeof(zeros)) {
		if (write(fd, zeros, sizeof(zeros)) < 0) {
			MMKVError(@"fail to write fd[%d], error:%s", fd, strerror(errno));
			return false;
		}
		size -= sizeof(zeros);
	}
	if (size > 0) {
		if (write(fd, zeros, size) < 0) {
			MMKVError(@"fail to write fd[%d], error:%s", fd, strerror(errno));
			return false;
		}
	}
	return true;
}
