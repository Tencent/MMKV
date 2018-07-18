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

MemoryFile::MemoryFile(NSString *path)
    : m_name(path), m_fd(-1), m_segmentPtr(nullptr), m_segmentSize(0) {
	m_fd = open(m_name.UTF8String, O_RDWR | O_CREAT, S_IRWXU);
	if (m_fd < 0) {
		MMKVError(@"fail to open:%@, %s", m_name, strerror(errno));
	} else {
		struct stat st = {};
		if (fstat(m_fd, &st) != -1) {
			m_segmentSize = static_cast<size_t>(st.st_size);
		}
		if (m_segmentSize < DEFAULT_MMAP_SIZE) {
			m_segmentSize = static_cast<size_t>(DEFAULT_MMAP_SIZE);
			if (ftruncate(m_fd, m_segmentSize) != 0 || !zeroFillFile(m_fd, 0, m_segmentSize)) {
				MMKVError(@"fail to truncate [%@] to size %zu, %s", m_name, m_segmentSize, strerror(errno));
				close(m_fd);
				m_fd = -1;
				removeFile(m_name);
				return;
			}
		}
		m_segmentPtr = (char *) mmap(nullptr, m_segmentSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
		if (m_segmentPtr == MAP_FAILED) {
			MMKVError(@"fail to mmap [%@], %s", m_name, strerror(errno));
			close(m_fd);
			m_fd = -1;
		}
	}
}

MemoryFile::~MemoryFile() {
	if (m_segmentPtr != MAP_FAILED && m_segmentPtr != nullptr) {
		munmap(m_segmentPtr, m_segmentSize);
		m_segmentPtr = nullptr;
	}
	if (m_fd >= 0) {
		close(m_fd);
		m_fd = -1;
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
	[fileAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication forKey:NSFileProtectionKey];
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
