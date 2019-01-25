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

#import "MMKV.h"
#import "AESCrypt.h"
#import "MMKVLog.h"
#import "MemoryFile.h"
#import "MiniCodedInputData.h"
#import "MiniCodedOutputData.h"
#import "MiniPBCoder.h"
#import "MiniPBUtility.h"
#import "ScopedLock.hpp"

#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
#import <UIKit/UIKit.h>
#endif

#include "aes/openssl/md5.h"
#import <algorithm>
#import <sys/mman.h>
#import <sys/stat.h>
#import <unistd.h>
#import <zlib.h>

static NSMutableDictionary *g_instanceDic;
static NSRecursiveLock *g_instanceLock;
id<MMKVHandler> g_callbackHandler;
bool g_isLogRedirecting = false;

#define DEFAULT_MMAP_ID @"mmkv.default"
#define CRC_FILE_SIZE DEFAULT_MMAP_SIZE
#define SPECIAL_CHARACTER_DIRECTORY_NAME @"specialCharacter"

static NSString *md5(NSString *value);
static NSString *encodeMmapID(NSString *mmapID);

@implementation MMKV {
	NSRecursiveLock *m_lock;
	NSMutableDictionary *m_dic;
	NSString *m_path;
	NSString *m_crcPath;
	NSString *m_mmapID;
	int m_fd;
	char *m_ptr;
	size_t m_size;
	size_t m_actualSize;
	MiniCodedOutputData *m_output;
	AESCrypt *m_cryptor;

	BOOL m_isInBackground;
	BOOL m_needLoadFromFile;
	BOOL m_hasFullWriteBack;

	uint32_t m_crcDigest;
	int m_crcFd;
	char *m_crcPtr;
}

#pragma mark - init

+ (void)initialize {
	if (self == MMKV.class) {
		g_instanceDic = [NSMutableDictionary dictionary];
		g_instanceLock = [[NSRecursiveLock alloc] init];

		MMKVInfo(@"pagesize:%d", DEFAULT_MMAP_SIZE);
	}
}

// a generic purpose instance
+ (instancetype)defaultMMKV {
	return [MMKV mmkvWithID:DEFAULT_MMAP_ID];
}

// any unique ID (com.tencent.xin.pay, etc)
+ (instancetype)mmkvWithID:(NSString *)mmapID {
	return [self mmkvWithID:mmapID cryptKey:nil];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey {
	return [self mmkvWithID:mmapID cryptKey:cryptKey relativePath:nil];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID relativePath:(nullable NSString *)path {
	return [self mmkvWithID:mmapID cryptKey:nil relativePath:path];
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey relativePath:(nullable NSString *)relativePath {
	if (mmapID.length <= 0) {
		return nil;
	}

	NSString *kvPath = [MMKV mappedKVPathWithID:mmapID relativePath:relativePath];
	if (!isFileExist(kvPath)) {
		if (!createFile(kvPath)) {
			MMKVError(@"fail to create file at %@", kvPath);
			return nil;
		}
	}
	NSString *kvKey = [MMKV mmapKeyWithMMapID:mmapID relativePath:relativePath];

	CScopedLock lock(g_instanceLock);

	MMKV *kv = [g_instanceDic objectForKey:kvKey];
	if (kv == nil) {
		kv = [[MMKV alloc] initWithMMapID:kvKey cryptKey:cryptKey path:kvPath];
		[g_instanceDic setObject:kv forKey:kvKey];
	}
	return kv;
}

- (instancetype)initWithMMapID:(NSString *)kvKey cryptKey:(NSData *)cryptKey path:(NSString *)path {
	if (self = [super init]) {
		m_lock = [[NSRecursiveLock alloc] init];

		m_mmapID = kvKey;
		m_path = path;
		m_crcPath = [MMKV crcPathWithMappedKVPath:m_path];

		if (cryptKey.length > 0) {
			m_cryptor = new AESCrypt((const unsigned char *) cryptKey.bytes, cryptKey.length);
		}

		[self loadFromFile];

#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
		auto appState = [UIApplication sharedApplication].applicationState;
		if (appState == UIApplicationStateBackground) {
			m_isInBackground = YES;
		} else {
			m_isInBackground = NO;
		}
		MMKVInfo(@"m_isInBackground:%d, appState:%ld", m_isInBackground, (long) appState);

		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onMemoryWarning) name:UIApplicationDidReceiveMemoryWarningNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didEnterBackground) name:UIApplicationDidEnterBackgroundNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didBecomeActive) name:UIApplicationDidBecomeActiveNotification object:nil];
#endif
	}
	return self;
}

- (void)dealloc {
	CScopedLock lock(m_lock);

	[[NSNotificationCenter defaultCenter] removeObserver:self];

	if (m_ptr != MAP_FAILED && m_ptr != nullptr) {
		munmap(m_ptr, m_size);
		m_ptr = nullptr;
	}
	if (m_fd >= 0) {
		close(m_fd);
		m_fd = -1;
	}
	if (m_output) {
		delete m_output;
		m_output = nullptr;
	}
	if (m_cryptor) {
		delete m_cryptor;
		m_cryptor = nullptr;
	}

	if (m_crcPtr != nullptr && m_crcPtr != MAP_FAILED) {
		munmap(m_crcPtr, CRC_FILE_SIZE);
		m_crcPtr = nullptr;
	}
	if (m_crcFd >= 0) {
		close(m_crcFd);
		m_crcFd = -1;
	}
}

#pragma mark - Application state

- (void)onMemoryWarning {
	CScopedLock lock(m_lock);

	MMKVInfo(@"cleaning on memory warning %@", m_mmapID);

	[self clearMemoryCache];
}

#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
- (void)didEnterBackground {
	CScopedLock lock(m_lock);

	m_isInBackground = YES;
	MMKVInfo(@"m_isInBackground:%d", m_isInBackground);
}

- (void)didBecomeActive {
	CScopedLock lock(m_lock);

	m_isInBackground = NO;
	MMKVInfo(@"m_isInBackground:%d", m_isInBackground);
}
#endif

#pragma mark - really dirty work

NSData *decryptBuffer(AESCrypt &crypter, NSData *inputBuffer) {
	size_t length = inputBuffer.length;
	NSMutableData *tmp = [NSMutableData dataWithLength:length];

	auto input = (const unsigned char *) inputBuffer.bytes;
	auto output = (unsigned char *) tmp.mutableBytes;
	crypter.decrypt(input, output, length);

	return tmp;
}

- (void)loadFromFile {
	m_fd = open(m_path.UTF8String, O_RDWR, S_IRWXU);
	if (m_fd < 0) {
		MMKVError(@"fail to open:%@, %s", m_path, strerror(errno));
	} else {
		m_size = 0;
		struct stat st = {};
		if (fstat(m_fd, &st) != -1) {
			m_size = (size_t) st.st_size;
		}
		// round up to (n * pagesize)
		if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
			m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1) * DEFAULT_MMAP_SIZE;
			if (ftruncate(m_fd, m_size) != 0) {
				MMKVError(@"fail to truncate [%@] to size %zu, %s", m_mmapID, m_size, strerror(errno));
				m_size = (size_t) st.st_size;
				return;
			}
		}
		m_ptr = (char *) mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
		if (m_ptr == MAP_FAILED) {
			MMKVError(@"fail to mmap [%@], %s", m_mmapID, strerror(errno));
		} else {
			const int offset = pbFixed32Size(0);
			NSData *lenBuffer = [NSData dataWithBytesNoCopy:m_ptr length:offset freeWhenDone:NO];
			@try {
				m_actualSize = MiniCodedInputData(lenBuffer).readFixed32();
			} @catch (NSException *exception) {
				MMKVError(@"%@", exception);
			}
			MMKVInfo(@"loading [%@] with %zu size in total, file size is %zu", m_mmapID, m_actualSize, m_size);
			if (m_actualSize > 0) {
				bool loadFromFile, needFullWriteback = false;
				if (m_actualSize < m_size && m_actualSize + offset <= m_size) {
					if ([self checkFileCRCValid] == YES) {
						loadFromFile = true;
					} else {
						loadFromFile = false;
						if (g_callbackHandler && [g_callbackHandler respondsToSelector:@selector(onMMKVCRCCheckFail:)]) {
							auto strategic = [g_callbackHandler onMMKVCRCCheckFail:m_mmapID];
							if (strategic == MMKVOnErrorRecover) {
								loadFromFile = true;
								needFullWriteback = true;
							}
						}
					}
				} else {
					MMKVError(@"load [%@] error: %zu size in total, file size is %zu", m_mmapID, m_actualSize, m_size);
					loadFromFile = false;
					if (g_callbackHandler && [g_callbackHandler respondsToSelector:@selector(onMMKVFileLengthError:)]) {
						auto strategic = [g_callbackHandler onMMKVFileLengthError:m_mmapID];
						if (strategic == MMKVOnErrorRecover) {
							loadFromFile = true;
							needFullWriteback = true;
							[self writeAcutalSize:m_size - offset];
						}
					}
				}
				if (loadFromFile) {
					NSData *inputBuffer = [NSData dataWithBytesNoCopy:m_ptr + offset length:m_actualSize freeWhenDone:NO];
					if (m_cryptor) {
						inputBuffer = decryptBuffer(*m_cryptor, inputBuffer);
					}
					m_dic = [MiniPBCoder decodeContainerOfClass:NSMutableDictionary.class withValueClass:NSData.class fromData:inputBuffer];
					m_output = new MiniCodedOutputData(m_ptr + offset + m_actualSize, m_size - offset - m_actualSize);
					if (needFullWriteback) {
						[self fullWriteback];
					}
				} else {
					[self writeAcutalSize:0];
					m_output = new MiniCodedOutputData(m_ptr + offset, m_size - offset);
					[self recaculateCRCDigest];
				}
			} else {
				m_output = new MiniCodedOutputData(m_ptr + offset, m_size - offset);
				[self recaculateCRCDigest];
			}
			MMKVInfo(@"loaded [%@] with %zu values", m_mmapID, (unsigned long) m_dic.count);
		}
	}
	if (m_dic == nil) {
		m_dic = [NSMutableDictionary dictionary];
	}

	if (![self isFileValid]) {
		MMKVWarning(@"[%@] file not valid", m_mmapID);
	}

	tryResetFileProtection(m_path);
	tryResetFileProtection(m_crcPath);
	m_needLoadFromFile = NO;
}

- (void)checkLoadData {
	//	CScopedLock lock(m_lock);

	if (m_needLoadFromFile == NO) {
		return;
	}
	m_needLoadFromFile = NO;
	[self loadFromFile];
}

- (void)clearAll {
	MMKVInfo(@"cleaning all values [%@]", m_mmapID);

	CScopedLock lock(m_lock);

	if (m_needLoadFromFile) {
		if (remove(m_path.UTF8String) != 0) {
			MMKVError(@"fail to remove file %@", m_mmapID);
		}
		m_needLoadFromFile = NO;
		[self loadFromFile];
		return;
	}

	[m_dic removeAllObjects];
	m_hasFullWriteBack = NO;

	if (m_output != nullptr) {
		delete m_output;
	}
	m_output = nullptr;

	if (m_ptr != nullptr && m_ptr != MAP_FAILED) {
		// for truncate
		size_t size = std::min<size_t>(DEFAULT_MMAP_SIZE, m_size);
		memset(m_ptr, 0, size);
		if (msync(m_ptr, size, MS_SYNC) != 0) {
			MMKVError(@"fail to msync [%@]:%s", m_mmapID, strerror(errno));
		}
		if (munmap(m_ptr, m_size) != 0) {
			MMKVError(@"fail to munmap [%@], %s", m_mmapID, strerror(errno));
		}
	}
	m_ptr = nullptr;

	if (m_fd >= 0) {
		if (m_size != DEFAULT_MMAP_SIZE) {
			MMKVInfo(@"truncating [%@] from %zu to %d", m_mmapID, m_size, DEFAULT_MMAP_SIZE);
			if (ftruncate(m_fd, DEFAULT_MMAP_SIZE) != 0) {
				MMKVError(@"fail to truncate [%@] to size %d, %s", m_mmapID, DEFAULT_MMAP_SIZE, strerror(errno));
			}
		}
		if (close(m_fd) != 0) {
			MMKVError(@"fail to close [%@], %s", m_mmapID, strerror(errno));
		}
	}
	m_fd = -1;
	m_size = 0;
	m_actualSize = 0;
	m_crcDigest = 0;

	if (m_cryptor) {
		m_cryptor->reset();
	}

	[self loadFromFile];
}

- (void)clearMemoryCache {
	CScopedLock lock(m_lock);

	if (m_needLoadFromFile) {
		MMKVInfo(@"ignore %@", m_mmapID);
		return;
	}
	m_needLoadFromFile = YES;

	[m_dic removeAllObjects];
	m_hasFullWriteBack = NO;

	if (m_output != nullptr) {
		delete m_output;
	}
	m_output = nullptr;

	if (m_ptr != nullptr && m_ptr != MAP_FAILED) {
		if (munmap(m_ptr, m_size) != 0) {
			MMKVError(@"fail to munmap [%@], %s", m_mmapID, strerror(errno));
		}
	}
	m_ptr = nullptr;

	if (m_fd >= 0) {
		if (close(m_fd) != 0) {
			MMKVError(@"fail to close [%@], %s", m_mmapID, strerror(errno));
		}
	}
	m_fd = -1;
	m_size = 0;
	m_actualSize = 0;

	if (m_cryptor) {
		m_cryptor->reset();
	}
}

- (void)close {
	CScopedLock g_lock(g_instanceLock);
	CScopedLock lock(m_lock);
	MMKVInfo(@"closing %@", m_mmapID);

	[self clearMemoryCache];

	[g_instanceDic removeObjectForKey:m_mmapID];
}

- (void)trim {
	CScopedLock lock(m_lock);
	MMKVInfo(@"prepare to trim %@", m_mmapID);

	[self checkLoadData];

	if (m_actualSize == 0) {
		[self clearAll];
		return;
	} else if (m_size <= DEFAULT_MMAP_SIZE) {
		return;
	}

	[self fullWriteback];
	auto oldSize = m_size;
	while (m_size > (m_actualSize * 2)) {
		m_size /= 2;
	}
	if (oldSize == m_size) {
		MMKVInfo(@"there's no need to trim %@ with size %zu, actualSize %zu", m_mmapID, m_size, m_actualSize);
		return;
	}

	MMKVInfo(@"trimming %@ from %zu to %zu", m_mmapID, oldSize, m_size);

	if (ftruncate(m_fd, m_size) != 0) {
		MMKVError(@"fail to truncate [%@] to size %zu, %s", m_mmapID, m_size, strerror(errno));
		m_size = oldSize;
		return;
	}
	if (munmap(m_ptr, oldSize) != 0) {
		MMKVError(@"fail to munmap [%@], %s", m_mmapID, strerror(errno));
	}
	m_ptr = (char *) mmap(m_ptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
	if (m_ptr == MAP_FAILED) {
		MMKVError(@"fail to mmap [%@], %s", m_mmapID, strerror(errno));
	}

	delete m_output;
	m_output = new MiniCodedOutputData(m_ptr + pbFixed32Size(0), m_size - pbFixed32Size(0));
	m_output->seek(m_actualSize);

	MMKVInfo(@"finish trim %@ to size %zu", m_mmapID, m_size);
}

- (BOOL)protectFromBackgroundWritting:(size_t)size writeBlock:(void (^)(MiniCodedOutputData *output))block {
	if (m_isInBackground) {
		static const int offset = pbFixed32Size(0);
		static const int pagesize = getpagesize();
		size_t realOffset = offset + m_actualSize - size;
		size_t pageOffset = (realOffset / pagesize) * pagesize;
		size_t pointerOffset = realOffset - pageOffset;
		size_t mmapSize = offset + m_actualSize - pageOffset;
		char *ptr = m_ptr + pageOffset;
		if (mlock(ptr, mmapSize) != 0) {
			MMKVError(@"fail to mlock [%@], %s", m_mmapID, strerror(errno));
			// just fail on this condition, otherwise app will crash anyway
			//block(m_output);
			return NO;
		} else {
			@try {
				MiniCodedOutputData output(ptr + pointerOffset, size);
				block(&output);
				m_output->seek(size);
			} @catch (NSException *exception) {
				MMKVError(@"%@", exception);
				return NO;
			} @finally {
				munlock(ptr, mmapSize);
			}
		}
	} else {
		block(m_output);
	}

	return YES;
}

// since we use append mode, when -[setData: forKey:] many times, space may not be enough
// try a full rewrite to make space
- (BOOL)ensureMemorySize:(size_t)newSize {
	[self checkLoadData];

	if (![self isFileValid]) {
		MMKVWarning(@"[%@] file not valid", m_mmapID);
		return NO;
	}

	if (newSize >= m_output->spaceLeft()) {
		// try a full rewrite to make space
		static const int offset = pbFixed32Size(0);
		NSData *data = [MiniPBCoder encodeDataWithObject:m_dic];
		size_t lenNeeded = data.length + offset + newSize;
		size_t avgItemSize = lenNeeded / std::max<size_t>(1, m_dic.count);
		size_t futureUsage = avgItemSize * std::max<size_t>(8, m_dic.count / 2);
		// 1. no space for a full rewrite, double it
		// 2. or space is not large enough for future usage, double it to avoid frequently full rewrite
		if (lenNeeded >= m_size || (lenNeeded + futureUsage) >= m_size) {
			size_t oldSize = m_size;
			do {
				m_size *= 2;
			} while (lenNeeded + futureUsage >= m_size);
			MMKVInfo(@"extending [%@] file size from %zu to %zu, incoming size:%zu, futrue usage:%zu",
			         m_mmapID, oldSize, m_size, newSize, futureUsage);

			// if we can't extend size, rollback to old state
			if (ftruncate(m_fd, m_size) != 0) {
				MMKVError(@"fail to truncate [%@] to size %zu, %s", m_mmapID, m_size, strerror(errno));
				m_size = oldSize;
				return NO;
			}

			if (munmap(m_ptr, oldSize) != 0) {
				MMKVError(@"fail to munmap [%@], %s", m_mmapID, strerror(errno));
			}
			m_ptr = (char *) mmap(m_ptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
			if (m_ptr == MAP_FAILED) {
				MMKVError(@"fail to mmap [%@], %s", m_mmapID, strerror(errno));
			}

			// check if we fail to make more space
			if (![self isFileValid]) {
				MMKVWarning(@"[%@] file not valid", m_mmapID);
				return NO;
			}
			// keep m_output consistent with m_ptr -- writeAcutalSize: may fail
			delete m_output;
			m_output = new MiniCodedOutputData(m_ptr + offset, m_size - offset);
			m_output->seek(m_actualSize);
		}

		if (m_cryptor) {
			m_cryptor->reset();
			auto ptr = (unsigned char *) data.bytes;
			m_cryptor->encrypt(ptr, ptr, data.length);
		}

		if ([self writeAcutalSize:data.length] == NO) {
			return NO;
		}

		delete m_output;
		m_output = new MiniCodedOutputData(m_ptr + offset, m_size - offset);
		BOOL ret = [self protectFromBackgroundWritting:m_actualSize
		                                    writeBlock:^(MiniCodedOutputData *output) {
			                                    output->writeRawData(data);
		                                    }];
		if (ret) {
			[self recaculateCRCDigest];
		}
		return ret;
	}
	return YES;
}

- (BOOL)writeAcutalSize:(size_t)actualSize {
	assert(m_ptr != 0);
	assert(m_ptr != MAP_FAILED);

	char *actualSizePtr = m_ptr;
	char *tmpPtr = nullptr;
	static const int offset = pbFixed32Size(0);

	if (m_isInBackground) {
		tmpPtr = m_ptr;
		if (mlock(tmpPtr, offset) != 0) {
			MMKVError(@"fail to mmap [%@], %d:%s", m_mmapID, errno, strerror(errno));
			// just fail on this condition, otherwise app will crash anyway
			return NO;
		} else {
			actualSizePtr = tmpPtr;
		}
	}

	@try {
		MiniCodedOutputData output(actualSizePtr, offset);
		output.writeFixed32((int32_t) actualSize);
	} @catch (NSException *exception) {
		MMKVError(@"%@", exception);
	}
	m_actualSize = actualSize;

	if (tmpPtr != nullptr && tmpPtr != MAP_FAILED) {
		munlock(tmpPtr, offset);
	}
	return YES;
}

- (BOOL)setRawData:(NSData *)data forKey:(NSString *)key {
	if (data.length <= 0 || key.length <= 0) {
		return NO;
	}
	CScopedLock lock(m_lock);

	[m_dic setObject:data forKey:key];
	m_hasFullWriteBack = NO;

	return [self appendData:data forKey:key];
}

- (BOOL)appendData:(NSData *)data forKey:(NSString *)key {
	size_t keyLength = [key lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	size_t size = keyLength + pbRawVarint32Size((int32_t) keyLength); // size needed to encode the key
	size += data.length + pbRawVarint32Size((int32_t) data.length);   // size needed to encode the value

	BOOL hasEnoughSize = [self ensureMemorySize:size];
	if (hasEnoughSize == NO || [self isFileValid] == NO) {
		return NO;
	}
	if (m_actualSize == 0) {
		NSData *allData = [MiniPBCoder encodeDataWithObject:m_dic];
		if (allData.length > 0) {
			if (m_cryptor) {
				m_cryptor->reset();
				auto ptr = (unsigned char *) allData.bytes;
				m_cryptor->encrypt(ptr, ptr, allData.length);
			}
			BOOL ret = [self writeAcutalSize:allData.length];
			if (ret) {
				ret = [self protectFromBackgroundWritting:m_actualSize
				                               writeBlock:^(MiniCodedOutputData *output) {
					                               output->writeRawData(allData); // note: don't write size of data
				                               }];
				if (ret) {
					[self recaculateCRCDigest];
				}
			}
			return ret;
		}
		return NO;
	} else {
		BOOL ret = [self writeAcutalSize:m_actualSize + size];
		if (ret) {
			static const int offset = pbFixed32Size(0);
			ret = [self protectFromBackgroundWritting:size
			                               writeBlock:^(MiniCodedOutputData *output) {
				                               output->writeString(key);
				                               output->writeData(data); // note: write size of data
			                               }];
			if (ret) {
				auto ptr = (uint8_t *) m_ptr + offset + m_actualSize - size;
				if (m_cryptor) {
					m_cryptor->encrypt(ptr, ptr, size);
				}
				[self updateCRCDigest:ptr withSize:size];
			}
		}
		return ret;
	}
}

- (NSData *)getRawDataForKey:(NSString *)key {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return [m_dic objectForKey:key];
}

- (BOOL)fullWriteback {
	CScopedLock lock(m_lock);
	if (m_needLoadFromFile) {
		return YES;
	}
	if (m_hasFullWriteBack) {
		return YES;
	}
	if (![self isFileValid]) {
		MMKVWarning(@"[%@] file not valid", m_mmapID);
		return NO;
	}

	if (m_dic.count == 0) {
		[self clearAll];
		return YES;
	}

	NSData *allData = [MiniPBCoder encodeDataWithObject:m_dic];
	if (allData.length > 0) {
		int offset = pbFixed32Size(0);
		if (allData.length + offset <= m_size) {
			if (m_cryptor) {
				m_cryptor->reset();
				auto ptr = (unsigned char *) allData.bytes;
				m_cryptor->encrypt(ptr, ptr, allData.length);
			}
			BOOL ret = [self writeAcutalSize:allData.length];
			if (ret) {
				delete m_output;
				m_output = new MiniCodedOutputData(m_ptr + offset, m_size - offset);
				ret = [self protectFromBackgroundWritting:m_actualSize
				                               writeBlock:^(MiniCodedOutputData *output) {
					                               output->writeRawData(allData); // note: don't write size of data
				                               }];
				if (ret) {
					[self recaculateCRCDigest];
					m_hasFullWriteBack = YES;
				}
			}
			return ret;
		} else {
			// ensureMemorySize will extend file & full rewrite, no need to write back again
			return [self ensureMemorySize:allData.length + offset - m_size];
		}
	}
	return NO;
}

- (BOOL)isFileValid {
	if (m_fd >= 0 && m_size > 0 && m_output != nullptr && m_ptr != nullptr && m_ptr != MAP_FAILED) {
		return YES;
	}
	//	MMKVWarning(@"[%@] file not valid", m_mmapID);
	return NO;
}

// assuming m_ptr & m_size is set
- (BOOL)checkFileCRCValid {
	if (m_ptr != nullptr && m_ptr != MAP_FAILED) {
		int offset = pbFixed32Size(0);
		m_crcDigest = (uint32_t) crc32(0, (const uint8_t *) m_ptr + offset, (uint32_t) m_actualSize);

		// for backward compatibility
		if (!isFileExist(m_crcPath)) {
			MMKVInfo(@"crc32 file not found:%@", m_crcPath);
			return YES;
		}
		NSData *oData = [NSData dataWithContentsOfFile:m_crcPath];
		uint32_t crc32 = 0;
		@try {
			MiniCodedInputData input(oData);
			crc32 = input.readFixed32();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
		if (m_crcDigest == crc32) {
			return YES;
		}
		MMKVError(@"check crc [%@] fail, crc32:%u, m_crcDigest:%u", m_mmapID, crc32, m_crcDigest);
	}
	return NO;
}

- (void)recaculateCRCDigest {
	if (m_ptr != nullptr && m_ptr != MAP_FAILED) {
		m_crcDigest = 0;
		int offset = pbFixed32Size(0);
		[self updateCRCDigest:(const uint8_t *) m_ptr + offset withSize:m_actualSize];
	}
}

- (void)updateCRCDigest:(const uint8_t *)ptr withSize:(size_t)length {
	if (ptr == nullptr) {
		return;
	}
	m_crcDigest = (uint32_t) crc32(m_crcDigest, ptr, (uint32_t) length);

	if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
		[self prepareCRCFile];
	}
	if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
		return;
	}

	static const size_t bufferLength = pbFixed32Size(0);
	if (m_isInBackground) {
		if (mlock(m_crcPtr, bufferLength) != 0) {
			MMKVError(@"fail to mlock crc [%@]-%p, %d:%s", m_mmapID, m_crcPtr, errno, strerror(errno));
			// just fail on this condition, otherwise app will crash anyway
			return;
		}
	}

	@try {
		MiniCodedOutputData output(m_crcPtr, bufferLength);
		output.writeFixed32((int32_t) m_crcDigest);
	} @catch (NSException *exception) {
		MMKVError(@"%@", exception);
	}
	if (m_isInBackground) {
		munlock(m_crcPtr, bufferLength);
	}
}

- (void)prepareCRCFile {
	if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
		if (!isFileExist(m_crcPath)) {
			createFile(m_crcPath);
		}
		m_crcFd = open(m_crcPath.UTF8String, O_RDWR, S_IRWXU);
		if (m_crcFd < 0) {
			MMKVError(@"fail to open:%@, %s", m_crcPath, strerror(errno));
			removeFile(m_crcPath);
		} else {
			size_t size = 0;
			struct stat st = {};
			if (fstat(m_crcFd, &st) != -1) {
				size = (size_t) st.st_size;
			}
			int fileLegth = CRC_FILE_SIZE;
			if (size != fileLegth) {
				size = fileLegth;
				if (ftruncate(m_crcFd, size) != 0) {
					MMKVError(@"fail to truncate [%@] to size %zu, %s", m_crcPath, size, strerror(errno));
					close(m_crcFd);
					m_crcFd = -1;
					removeFile(m_crcPath);
					return;
				}
			}
			m_crcPtr = (char *) mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_crcFd, 0);
			if (m_crcPtr == MAP_FAILED) {
				MMKVError(@"fail to mmap [%@], %s", m_crcPath, strerror(errno));
				close(m_crcFd);
				m_crcFd = -1;
			}
		}
	}
}

#pragma mark - encryption & decryption

- (nullable NSData *)cryptKey {
	if (m_cryptor) {
		NSMutableData *data = [NSMutableData dataWithLength:AES_KEY_LEN];
		m_cryptor->getKey(data.mutableBytes);
		return data;
	}
	return NULL;
}

- (BOOL)reKey:(NSData *)newKey {
	CScopedLock lock(m_lock);
	[self checkLoadData];

	if (m_cryptor) {
		if (newKey.length > 0) {
			NSData *oldKey = [self cryptKey];
			if ([newKey isEqualToData:oldKey]) {
				return YES;
			} else {
				// change encryption key
				MMKVInfo(@"reKey with new aes key");
				delete m_cryptor;
				auto ptr = (const unsigned char *) newKey.bytes;
				m_cryptor = new AESCrypt(ptr, newKey.length);
				return [self fullWriteback];
			}
		} else {
			// decryption to plain text
			MMKVInfo(@"reKey with no aes key");
			delete m_cryptor;
			m_cryptor = nullptr;
			return [self fullWriteback];
		}
	} else {
		if (newKey.length > 0) {
			// transform plain text to encrypted text
			MMKVInfo(@"reKey with aes key");
			auto ptr = (const unsigned char *) newKey.bytes;
			m_cryptor = new AESCrypt(ptr, newKey.length);
			return [self fullWriteback];
		} else {
			return YES;
		}
	}
	return NO;
}

#pragma mark - set & get

- (BOOL)setObject:(nullable NSObject<NSCoding> *)object forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	if (object == nil) {
		[self removeValueForKey:key];
		return YES;
	}

	NSData *data;
	if ([MiniPBCoder isMiniPBCoderCompatibleObject:object]) {
		data = [MiniPBCoder encodeDataWithObject:object];
	} else {
		/*if ([object conformsToProtocol:@protocol(NSCoding)])*/ {
			data = [NSKeyedArchiver archivedDataWithRootObject:object];
		}
	}

	return [self setRawData:data forKey:key];
}

- (BOOL)setBool:(BOOL)value forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	size_t size = pbBoolSize(value);
	NSMutableData *data = [NSMutableData dataWithLength:size];
	MiniCodedOutputData output(data);
	output.writeBool(value);

	return [self setRawData:data forKey:key];
}

- (BOOL)setInt32:(int32_t)value forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	size_t size = pbInt32Size(value);
	NSMutableData *data = [NSMutableData dataWithLength:size];
	MiniCodedOutputData output(data);
	output.writeInt32(value);

	return [self setRawData:data forKey:key];
}

- (BOOL)setUInt32:(uint32_t)value forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	size_t size = pbUInt32Size(value);
	NSMutableData *data = [NSMutableData dataWithLength:size];
	MiniCodedOutputData output(data);
	output.writeUInt32(value);

	return [self setRawData:data forKey:key];
}

- (BOOL)setInt64:(int64_t)value forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	size_t size = pbInt64Size(value);
	NSMutableData *data = [NSMutableData dataWithLength:size];
	MiniCodedOutputData output(data);
	output.writeInt64(value);

	return [self setRawData:data forKey:key];
}

- (BOOL)setUInt64:(uint64_t)value forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	size_t size = pbUInt64Size(value);
	NSMutableData *data = [NSMutableData dataWithLength:size];
	MiniCodedOutputData output(data);
	output.writeUInt64(value);

	return [self setRawData:data forKey:key];
}

- (BOOL)setFloat:(float)value forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	size_t size = pbFloatSize(value);
	NSMutableData *data = [NSMutableData dataWithLength:size];
	MiniCodedOutputData output(data);
	output.writeFloat(value);

	return [self setRawData:data forKey:key];
}

- (BOOL)setDouble:(double)value forKey:(NSString *)key {
	if (key.length <= 0) {
		return NO;
	}
	size_t size = pbDoubleSize(value);
	NSMutableData *data = [NSMutableData dataWithLength:size];
	MiniCodedOutputData output(data);
	output.writeDouble(value);

	return [self setRawData:data forKey:key];
}

- (BOOL)setString:(NSString *)value forKey:(NSString *)key {
	return [self setObject:value forKey:key];
}

- (BOOL)setDate:(NSDate *)value forKey:(NSString *)key {
	return [self setObject:value forKey:key];
}

- (BOOL)setData:(NSData *)value forKey:(NSString *)key {
	return [self setObject:value forKey:key];
}

- (id)getObjectOfClass:(Class)cls forKey:(NSString *)key {
	if (key.length <= 0) {
		return nil;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {

		if ([MiniPBCoder isMiniPBCoderCompatibleType:cls]) {
			return [MiniPBCoder decodeObjectOfClass:cls fromData:data];
		} else {
			if ([cls conformsToProtocol:@protocol(NSCoding)]) {
				return [NSKeyedUnarchiver unarchiveObjectWithData:data];
			}
		}
	}
	return nil;
}

- (BOOL)getBoolForKey:(NSString *)key {
	return [self getBoolForKey:key defaultValue:FALSE];
}
- (BOOL)getBoolForKey:(NSString *)key defaultValue:(BOOL)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readBool();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (int32_t)getInt32ForKey:(NSString *)key {
	return [self getInt32ForKey:key defaultValue:0];
}
- (int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readInt32();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (uint32_t)getUInt32ForKey:(NSString *)key {
	return [self getUInt32ForKey:key defaultValue:0];
}
- (uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readUInt32();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (int64_t)getInt64ForKey:(NSString *)key {
	return [self getInt64ForKey:key defaultValue:0];
}
- (int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readInt64();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (uint64_t)getUInt64ForKey:(NSString *)key {
	return [self getUInt64ForKey:key defaultValue:0];
}
- (uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readUInt64();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (float)getFloatForKey:(NSString *)key {
	return [self getFloatForKey:key defaultValue:0];
}
- (float)getFloatForKey:(NSString *)key defaultValue:(float)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readFloat();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (double)getDoubleForKey:(NSString *)key {
	return [self getDoubleForKey:key defaultValue:0];
}
- (double)getDoubleForKey:(NSString *)key defaultValue:(double)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData *data = [self getRawDataForKey:key];
	if (data.length > 0) {
		@try {
			MiniCodedInputData input(data);
			return input.readDouble();
		} @catch (NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

- (nullable NSString *)getStringForKey:(NSString *)key {
	return [self getStringForKey:key defaultValue:nil];
}
- (nullable NSString *)getStringForKey:(NSString *)key defaultValue:(nullable NSString *)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	return [self getObjectOfClass:NSString.class forKey:key];
}

- (nullable NSDate *)getDateForKey:(NSString *)key {
	return [self getDateForKey:key defaultValue:nil];
}
- (nullable NSDate *)getDateForKey:(NSString *)key defaultValue:(nullable NSDate *)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	return [self getObjectOfClass:NSDate.class forKey:key];
}

- (nullable NSData *)getDataForKey:(NSString *)key {
	return [self getDataForKey:key defaultValue:nil];
}
- (nullable NSData *)getDataForKey:(NSString *)key defaultValue:(nullable NSData *)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	return [self getObjectOfClass:NSData.class forKey:key];
}

- (size_t)getValueSizeForKey:(NSString *)key NS_SWIFT_NAME(valueSize(forKey:)) {
	if (key.length <= 0) {
		return 0;
	}
	NSData *data = [self getRawDataForKey:key];
	return data.length;
}

#pragma mark - enumerate

- (BOOL)containsKey:(NSString *)key {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return ([m_dic objectForKey:key] != nil);
}

- (size_t)count {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return m_dic.count;
}

- (size_t)totalSize {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return m_size;
}

- (size_t)actualSize {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return m_actualSize;
}

- (void)enumerateKeys:(void (^)(NSString *key, BOOL *stop))block {
	if (block == nil) {
		return;
	}
	MMKVInfo(@"enumerate [%@] begin", m_mmapID);
	CScopedLock lock(m_lock);
	[self checkLoadData];
	[m_dic enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
		block(key, stop);
	}];
	MMKVInfo(@"enumerate [%@] finish", m_mmapID);
}

- (void)removeValueForKey:(NSString *)key {
	if (key.length <= 0) {
		return;
	}
	CScopedLock lock(m_lock);
	[self checkLoadData];

	[m_dic removeObjectForKey:key];
	m_hasFullWriteBack = NO;

	static NSData *data = [NSData data];
	[self appendData:data forKey:key];
}

- (void)removeValuesForKeys:(NSArray *)arrKeys {
	if (arrKeys.count <= 0) {
		return;
	}
	CScopedLock lock(m_lock);
	[self checkLoadData];

	[m_dic removeObjectsForKeys:arrKeys];
	m_hasFullWriteBack = NO;

	MMKVInfo(@"remove [%@] %lu keys, %lu remain", m_mmapID, (unsigned long) arrKeys.count, (unsigned long) m_dic.count);

	[self fullWriteback];
}

#pragma mark - Boring stuff

- (void)sync {
	CScopedLock lock(m_lock);
	if (m_needLoadFromFile || ![self isFileValid] || m_crcPtr == nullptr) {
		return;
	}
	if (msync(m_ptr, m_actualSize, MS_SYNC) != 0) {
		MMKVError(@"fail to msync data file of [%@]:%s", m_mmapID, strerror(errno));
	}
	if (msync(m_crcPtr, CRC_FILE_SIZE, MS_SYNC) != 0) {
		MMKVError(@"fail to msync crc-32 file of [%@]:%s", m_mmapID, strerror(errno));
	}
}

static NSString *g_basePath = nil;
+ (NSString *)mmkvBasePath {
	if (g_basePath.length > 0) {
		return g_basePath;
	}

	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *documentPath = (NSString *) [paths firstObject];
	if ([documentPath length] > 0) {
		g_basePath = [documentPath stringByAppendingPathComponent:@"mmkv"];
		return g_basePath;
	} else {
		return @"";
	}
}

+ (void)setMMKVBasePath:(NSString *)basePath {
	if (basePath.length > 0) {
		g_basePath = basePath;
		MMKVInfo(@"set MMKV base path to: %@", g_basePath);
	}
}

+ (NSString *)mmapKeyWithMMapID:(NSString *)mmapID relativePath:(nullable NSString *)relativePath {
	NSString *string = nil;
	if ([relativePath length] > 0 && [relativePath isEqualToString:[MMKV mmkvBasePath]] == NO) {
		string = md5([relativePath stringByAppendingPathComponent:mmapID]);
	} else {
		string = mmapID;
	}
	MMKVInfo(@"mmapKey: %@", string);
	return string;
}

+ (NSString *)mappedKVPathWithID:(NSString *)mmapID relativePath:(nullable NSString *)path {
	NSString *basePath = nil;
	if ([path length] > 0) {
		basePath = path;
	} else {
		basePath = [self mmkvBasePath];
	}

	if ([basePath length] > 0) {
		NSString *mmapIDstring = encodeMmapID(mmapID);
		return [basePath stringByAppendingPathComponent:mmapIDstring];
	} else {
		return @"";
	}
}

+ (NSString *)crcPathWithMappedKVPath:(NSString *)kvPath {
	return [kvPath stringByAppendingString:@".crc"];
}

+ (BOOL)isFileValid:(NSString *)mmapID {
	return [self isFileValid:mmapID relativePath:nil];
}

+ (BOOL)isFileValid:(NSString *)mmapID relativePath:(nullable NSString *)path {
	NSFileManager *fileManager = [NSFileManager defaultManager];

	NSString *kvPath = [self mappedKVPathWithID:mmapID relativePath:path];
	if ([fileManager fileExistsAtPath:kvPath] == NO) {
		// kv file not exist
		return YES;
	}

	NSString *crcPath = [self crcPathWithMappedKVPath:kvPath];
	if ([fileManager fileExistsAtPath:crcPath] == NO) {
		// crc file not exist
		return YES;
	}

	NSData *fileData = [NSData dataWithContentsOfFile:crcPath];
	uint32_t crcFile = 0;
	@try {
		MiniCodedInputData input(fileData);
		crcFile = input.readFixed32();
	} @catch (NSException *exception) {
		return NO;
	}

	const int offset = pbFixed32Size(0);
	size_t actualSize = 0;
	fileData = [NSData dataWithContentsOfFile:kvPath];
	@try {
		actualSize = MiniCodedInputData(fileData).readFixed32();
	} @catch (NSException *exception) {
		return NO;
	}

	if (actualSize > fileData.length - offset) {
		return NO;
	}

	uint32_t crcDigest = (uint32_t) crc32(0, (const uint8_t *) fileData.bytes + offset, (uint32_t) actualSize);

	return crcFile == crcDigest;
}

+ (void)registerHandler:(id<MMKVHandler>)handler {
	CScopedLock lock(g_instanceLock);
	g_callbackHandler = handler;

	if ([g_callbackHandler respondsToSelector:@selector(mmkvLogWithLevel:file:line:func:message:)]) {
		g_isLogRedirecting = true;

		// some logging before registerHandler
		MMKVInfo(@"pagesize:%d", DEFAULT_MMAP_SIZE);
	}
}

+ (void)unregiserHandler {
	CScopedLock lock(g_instanceLock);
	g_callbackHandler = nil;
	g_isLogRedirecting = false;
}

+ (void)setLogLevel:(MMKVLogLevel)logLevel {
	CScopedLock lock(g_instanceLock);
	g_currentLogLevel = logLevel;
}

- (uint32_t)migrateFromUserDefaults:(NSUserDefaults *)userDaults {
	NSDictionary *dic = [userDaults dictionaryRepresentation];
	if (dic.count <= 0) {
		MMKVInfo(@"migrate data fail, userDaults is nil or empty");
		return 0;
	}
	__block uint32_t count = 0;
	[dic enumerateKeysAndObjectsUsingBlock:^(id _Nonnull key, id _Nonnull obj, BOOL *_Nonnull stop) {
		if ([key isKindOfClass:[NSString class]]) {
			NSString *stringKey = key;
			if ([MMKV tranlateData:obj key:stringKey kv:self]) {
				count++;
			}
		} else {
			MMKVWarning(@"unknown type of key:%@", key);
		}
	}];
	return count;
}

+ (BOOL)tranlateData:(id)obj key:(NSString *)key kv:(MMKV *)kv {
	if ([obj isKindOfClass:[NSString class]]) {
		return [kv setString:obj forKey:key];
	} else if ([obj isKindOfClass:[NSData class]]) {
		return [kv setData:obj forKey:key];
	} else if ([obj isKindOfClass:[NSDate class]]) {
		return [kv setDate:obj forKey:key];
	} else if ([obj isKindOfClass:[NSNumber class]]) {
		NSNumber *num = obj;
		CFNumberType numberType = CFNumberGetType((CFNumberRef) obj);
		switch (numberType) {
			case kCFNumberCharType:
			case kCFNumberSInt8Type:
			case kCFNumberSInt16Type:
			case kCFNumberSInt32Type:
			case kCFNumberIntType:
			case kCFNumberShortType:
				return [kv setInt32:num.intValue forKey:key];
			case kCFNumberSInt64Type:
			case kCFNumberLongType:
			case kCFNumberNSIntegerType:
			case kCFNumberLongLongType:
				return [kv setInt64:num.longLongValue forKey:key];
			case kCFNumberFloat32Type:
				return [kv setFloat:num.floatValue forKey:key];
			case kCFNumberFloat64Type:
			case kCFNumberDoubleType:
				return [kv setDouble:num.doubleValue forKey:key];
			default:
				MMKVWarning(@"unknown number type:%ld, key:%@", (long) numberType, key);
				return NO;
		}
	} else if ([obj isKindOfClass:[NSArray class]] || [obj isKindOfClass:[NSDictionary class]]) {
		return [kv setObject:obj forKey:key];
	} else {
		MMKVWarning(@"unknown type of key:%@", key);
	}
	return NO;
}

@end

static NSString *md5(NSString *value) {
	unsigned char md[MD5_DIGEST_LENGTH] = {0};
	char tmp[3] = {0}, buf[33] = {0};
	MD5((unsigned char *) value.UTF8String, [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding], md);
	for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
		sprintf(tmp, "%2.2x", md[i]);
		strcat(buf, tmp);
	}
	return [NSString stringWithCString:buf encoding:NSASCIIStringEncoding];
}

static NSString *encodeMmapID(NSString *mmapID) {
	static NSCharacterSet *specialCharacters = [NSCharacterSet characterSetWithCharactersInString:@"\\/:*?\"<>|"];
	auto range = [mmapID rangeOfCharacterFromSet:specialCharacters];
	if (range.location != NSNotFound) {
		NSString *encodedID = md5(mmapID);
		return [SPECIAL_CHARACTER_DIRECTORY_NAME stringByAppendingFormat:@"/%@", encodedID];
	} else {
		return mmapID;
	}
}
