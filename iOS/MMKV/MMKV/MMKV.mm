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

#import "MMKV+Internal.h"

static NSMutableDictionary *g_instanceDic;
static NSRecursiveLock *g_instanceLock;

#define DEFAULT_MMAP_ID @"mmkv.default"
#define CRC_FILE_SIZE DEFAULT_MMAP_SIZE

@implementation MMKV

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
	if (mmapID.length <= 0) {
		return nil;
	}
	CScopedLock lock(g_instanceLock);

	MMKV *kv = [g_instanceDic objectForKey:mmapID];
	if (kv == nil) {
		kv = [[MMKV alloc] initWithMMapID:mmapID cryptKey:nil];
		[g_instanceDic setObject:kv forKey:mmapID];
	}
	return kv;
}

+ (instancetype)mmkvWithID:(NSString *)mmapID cryptKey:(NSData *)cryptKey {
	if (mmapID.length <= 0) {
		return nil;
	}
	CScopedLock lock(g_instanceLock);

	MMKV *kv = [g_instanceDic objectForKey:mmapID];
	if (kv == nil) {
		kv = [[MMKV alloc] initWithMMapID:mmapID cryptKey:cryptKey];
		[g_instanceDic setObject:kv forKey:mmapID];
	}
	return kv;
}

- (instancetype)initWithMMapID:(NSString *)mmapID cryptKey:(NSData *)cryptKey {
	if (self = [super init]) {
		m_lock = [[NSRecursiveLock alloc] init];

		m_mmapID = mmapID;

		m_path = [MMKV mappedKVPathWithID:m_mmapID];
		if (!isFileExist(m_path)) {
			createFile(m_path);
		}
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

	if (m_memoryFile) {
		delete m_memoryFile;
		m_memoryFile = nullptr;
		m_headSegmemt = nullptr;
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

	if (m_needLoadFromFile) {
		MMKVInfo(@"ignore %@", m_mmapID);
		return;
	}
	m_needLoadFromFile = YES;

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
	m_memoryFile = new MemoryFile(m_path);
	if (m_memoryFile->getFd() <= 0) {
		MMKVError(@"fail to open:%@, %s", m_path, strerror(errno));
	} else {
		m_size = m_memoryFile->getFileSize();
		m_headSegmemt = m_memoryFile->tryGetSegment(0);
		if (!m_headSegmemt || m_headSegmemt->ptr == MAP_FAILED) {
			MMKVError(@"fail to mmap [%@], %s", m_mmapID, strerror(errno));
		} else {
			memcpy(&m_actualSize, m_headSegmemt->ptr, Fixed32Size);
			MMKVInfo(@"loading [%@] with %zu size in total, file size is %zu", m_mmapID, m_actualSize, m_size);
			if (m_actualSize > 0) {
				if (m_actualSize < m_size && m_actualSize + Fixed32Size <= m_size) {
					if ([self checkFileCRCValid] == YES) {
						[MiniPBCoder decodeContainer:m_kvItemsWrap fromMemoryFile:m_memoryFile fromOffset:Fixed32Size withSize:Fixed32Size + m_actualSize];
						m_output = new MiniCodedOutputData(m_memoryFile, Fixed32Size + m_actualSize);
					} else {
						[self writeAcutalSize:0];
						m_output = new MiniCodedOutputData(m_memoryFile, Fixed32Size);
						[self recaculateCRCDigest];
					}
				} else {
					MMKVError(@"load [%@] error: %zu size in total, file size is %zu", m_mmapID, m_actualSize, m_size);
					[self writeAcutalSize:0];
					m_output = new MiniCodedOutputData(m_memoryFile, Fixed32Size);
					[self recaculateCRCDigest];
				}
			} else {
				m_output = new MiniCodedOutputData(m_memoryFile, Fixed32Size);
				[self recaculateCRCDigest];
			}
			MMKVInfo(@"loaded [%@] with %zu values", m_mmapID, m_kvItemsWrap.size());
		}
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

	m_kvItemsWrap.clear();

	if (m_output != nullptr) {
		delete m_output;
	}
	m_output = nullptr;

	if (m_headSegmemt && m_headSegmemt->ptr != MAP_FAILED) {
		// for truncate
		size_t size = std::min<size_t>(DEFAULT_MMAP_SIZE, m_size);
		memset(m_headSegmemt->ptr, 0, size);
		if (msync(m_headSegmemt->ptr, size, MS_SYNC) != 0) {
			MMKVError(@"fail to msync [%@]:%s", m_mmapID, strerror(errno));
		}
	}
	m_headSegmemt = nullptr;

	if (m_memoryFile->getFd() >= 0) {
		if (m_size != DEFAULT_MMAP_SIZE) {
			MMKVInfo(@"truncating [%@] from %zu to %d", m_mmapID, m_size, DEFAULT_MMAP_SIZE);
			if (!m_memoryFile->truncate(DEFAULT_MMAP_SIZE)) {
				MMKVError(@"fail to truncate [%@] to size %d, %s", m_mmapID, DEFAULT_MMAP_SIZE, strerror(errno));
			}
		}
	}
	delete m_memoryFile;
	m_memoryFile = nullptr;
	m_size = 0;
	m_actualSize = 0;
	m_crcDigest = 0;

	[self loadFromFile];
}

- (void)clearMemoryCache {
	m_kvItemsWrap.clear();

	if (m_output != nullptr) {
		delete m_output;
	}
	m_output = nullptr;

	m_headSegmemt = nullptr;

	delete m_memoryFile;
	m_memoryFile = nullptr;
	m_size = 0;
	m_actualSize = 0;
}
/*
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
}*/

// since we use append mode, when -[setData: forKey:] many times, space may not be enough
// try a full rewrite to make space
- (BOOL)ensureMemorySize:(size_t)newSize {
	[self checkLoadData];

	if (![self isFileValid]) {
		MMKVWarning(@"[%@] file not valid", m_mmapID);
		return NO;
	}
	if (newSize <= m_output->spaceLeft()) {
		return YES;
	}
	// avoid frequently full rewrite
	auto avgItemSize = (m_actualSize + newSize) / (m_kvItemsWrap.size() + 1);
	auto futureUsage = avgItemSize * std::max<size_t>(8, (m_kvItemsWrap.size() + 1) / 2);
	if (![self fullWriteback]) {
		MMKVError(@"fail to fullWriteback [%@] before truncate", m_mmapID);
		return NO;
	} else if (newSize + futureUsage <= m_output->spaceLeft()) {
		return YES;
	}
	// try truncate() to make space
	auto lenNeeded = m_actualSize + Fixed32Size + newSize;
	// if space is not large enough for future usage, double it
	auto oldSize = m_size;
	do {
		m_size *= 2;
	} while (lenNeeded + futureUsage >= m_size);
	if (m_size > std::numeric_limits<uint32_t>::max()) {
		MMKVWarning(@"reaching MMKV size limit %u, this is the best we can do", std::numeric_limits<uint32_t>::max());
		m_size = std::numeric_limits<uint32_t>::max();
		if (m_size == oldSize) {
			return newSize <= m_output->spaceLeft();
		}
	}
	MMKVInfo(@"extending [%@] file size from %zu to %zu, incoming size:%zu, futrue usage:%zu, space left:%zu",
	         m_mmapID, oldSize, m_size, newSize, futureUsage, m_output->spaceLeft());

	// if we can't extend size, rollback to old state
	if (!m_memoryFile->truncate(m_size)) {
		MMKVError(@"fail to truncate [%@] to size %zu, %s", m_mmapID, m_size, strerror(errno));
		m_size = oldSize;
		return NO;
	}
	delete m_output;
	m_output = new MiniCodedOutputData(m_memoryFile, Fixed32Size + m_actualSize);
	return YES;
}

- (BOOL)fullWriteback {
	CScopedLock lock(m_lock);
	if (![self isFileValid]) {
		MMKVWarning(@"[%@] file not valid", m_mmapID);
		return NO;
	}
	if (m_kvItemsWrap.size() == 0) {
		[self clearAll];
		return YES;
	}
#ifdef DEBUG
	MMKVInfo(@"begin fullWriteback all values [%@]", m_mmapID);
	NSDate *startDate = [NSDate date];
#endif
	auto arrMergedItems = m_kvItemsWrap.mergeNearbyItems();
	size_t itemSize = 0;
	for (auto &segment : arrMergedItems) {
		auto &first = m_kvItemsWrap[segment.first], &last = m_kvItemsWrap[segment.second];
		itemSize += last.end() - first.offset;
	}
	auto sizeNeeded = Fixed32Size + itemSize + ItemSizeHolderSize;
	if (sizeNeeded > m_size) {
		MMKVError(@"this should never happen, sizeNeeded %zu > m_size %zu", sizeNeeded, m_size);
		return NO;
	}

	// TODO: some basic rollback logic and/or error recover logic
	if (![self writeAcutalSize:itemSize + ItemSizeHolderSize]) {
		return NO;
	}
	// TODO: background mlock
	MiniCodedOutputData(m_headSegmemt->ptr + Fixed32Size, Fixed32Size).writeInt32(ItemSizeHolder);
	size_t position = Fixed32Size + ItemSizeHolderSize;
	for (auto segment : arrMergedItems) {
		auto &first = m_kvItemsWrap[segment.first], &last = m_kvItemsWrap[segment.second];
		auto size = last.end() - first.offset;
		if (!m_memoryFile->memmove(position, first.offset, size)) {
			MMKVError(@"fail to move to position %zu from %u with size %zu", position, first.offset, size);
			return NO;
		}
		auto diff = first.offset - position;
		for (size_t index = segment.first; index <= segment.second; index++) {
			auto &kvHolder = m_kvItemsWrap[index];
			kvHolder.offset -= diff;
		}
		position += size;
	}

	delete m_output;
	m_output = new MiniCodedOutputData(m_memoryFile, sizeNeeded);

	[self recaculateCRCDigest];
#ifdef DEBUG
	NSDate *endDate = [NSDate date];
	int cost = [endDate timeIntervalSinceDate:startDate] * 1000;
	MMKVInfo(@"done fullWriteback all values [%@], %d ms", m_mmapID, cost);
#endif
	return YES;
}

- (BOOL)writeAcutalSize:(size_t)actualSize {
	assert(m_headSegmemt);
	assert(m_headSegmemt->ptr != MAP_FAILED);

	auto actualSizePtr = m_headSegmemt->ptr;

	if (m_isInBackground) {
		if (mlock(actualSizePtr, DEFAULT_MMAP_SIZE) != 0) {
			MMKVError(@"fail to mmap [%@], %d:%s", m_mmapID, errno, strerror(errno));
			// just fail on this condition, otherwise app will crash anyway
			return NO;
		}
	}
	{
		uint32_t size = static_cast<uint32_t>(actualSize);
		memcpy(actualSizePtr, &size, sizeof(uint32_t));
	}
	m_actualSize = actualSize;

	if (m_isInBackground) {
		munlock(actualSizePtr, DEFAULT_MMAP_SIZE);
	}
	return YES;
}

- (BOOL)isFileValid {
	if (m_memoryFile && m_memoryFile->getFd() >= 0 && m_size > 0 && m_output != nullptr && m_headSegmemt && m_headSegmemt->ptr != MAP_FAILED) {
		return YES;
	}
	//MMKVWarning(@"[%@] file not valid", m_mmapID);
	return NO;
}

- (BOOL)checkFileCRCValid {
	if (m_memoryFile) {
		m_crcDigest = m_memoryFile->crc32(0, Fixed32Size, m_actualSize);

		if (!isFileExist(m_crcPath)) {
			MMKVInfo(@"crc32 file not found:%@", m_crcPath);
			return NO;
		}
		NSData *oData = [NSData dataWithContentsOfFile:m_crcPath];
		uint32_t crc32 = 0;
		memcpy(&crc32, oData.bytes, Fixed32Size);
		if (m_crcDigest == crc32) {
			return YES;
		}
		MMKVError(@"check crc [%@] fail, crc32:%u, m_crcDigest:%u", m_mmapID, crc32, m_crcDigest);
	}
	return NO;
}

- (void)recaculateCRCDigest {
	m_crcDigest = 0;
	m_crcDigest = m_memoryFile->crc32(m_crcDigest, Fixed32Size, m_actualSize);

	[self writeBackCRCDigest];
}

- (void)writeBackCRCDigest {
	if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
		[self prepareCRCFile];
	}
	if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
		return;
	}

	if (m_isInBackground) {
		if (mlock(m_crcPtr, Fixed32Size) != 0) {
			MMKVError(@"fail to mlock crc [%@]-%p, %d:%s", m_mmapID, m_crcPtr, errno, strerror(errno));
			// just fail on this condition, otherwise app will crash anyway
			return;
		}
	}

	memcpy(m_crcPtr, &m_crcDigest, Fixed32Size);

	if (m_isInBackground) {
		munlock(m_crcPtr, Fixed32Size);
	}
}

- (void)prepareCRCFile {
	if (m_crcPtr == nullptr || m_crcPtr == MAP_FAILED) {
		if (!isFileExist(m_crcPath)) {
			createFile(m_crcPath);
		}
		m_crcFd = open(m_crcPath.UTF8String, O_RDWR, S_IRWXU);
		if (m_crcFd <= 0) {
			MMKVError(@"fail to open:%@, %s", m_crcPath, strerror(errno));
			removeFile(m_crcPath);
		} else {
			size_t size = 0;
			struct stat st = {};
			if (fstat(m_crcFd, &st) != -1) {
				size = (size_t) st.st_size;
			}
			int fileLegth = CRC_FILE_SIZE;
			if (size < fileLegth) {
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

#pragma mark - enumerate

- (BOOL)containsKey:(NSString *)key {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return (m_kvItemsWrap.find(key) != nullptr);
}

- (size_t)count {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return m_kvItemsWrap.size();
}

- (size_t)totalSize {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return m_size;
}

- (void)enumerateKeys:(void (^)(NSString *key, BOOL *stop))block {
	if (block == nil) {
		return;
	}
	MMKVInfo(@"enumerate [%@] begin", m_mmapID);
	CScopedLock lock(m_lock);
	[self checkLoadData];

	// TODO: enumerateKeys
	/*for (auto &kv : m_dic) {
     BOOL stop = NO;
     
     NSString *key = [[NSString alloc] initWithBytes:kv.first.getPtr() length:kv.first.length() encoding:NSUTF8StringEncoding];
     block(key, &stop);
     
     if (stop) {
     break;
     }
     }*/
	MMKVInfo(@"enumerate [%@] finish", m_mmapID);
}

#pragma mark - Boring stuff

- (void)sync {
	CScopedLock lock(m_lock);
	if (m_needLoadFromFile || ![self isFileValid] || m_crcPtr == nullptr) {
		return;
	}

	m_memoryFile->sync(MS_SYNC);

	if (msync(m_crcPtr, CRC_FILE_SIZE, MS_SYNC) != 0) {
		MMKVError(@"fail to msync crc-32 file of [%@]:%s", m_mmapID, strerror(errno));
	}
}

+ (NSString *)mappedKVPathWithID:(NSString *)mmapID {
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *nsLibraryPath = (NSString *) [paths firstObject];
	if ([nsLibraryPath length] > 0) {
		return [nsLibraryPath stringByAppendingFormat:@"/mmkv/%@", mmapID];
	} else {
		return @"";
	}
}

+ (NSString *)crcPathWithMappedKVPath:(NSString *)kvPath {
	return [kvPath stringByAppendingString:@".crc"];
}

+ (BOOL)isFileValid:(NSString *)mmapID {
	NSFileManager *fileManager = [NSFileManager defaultManager];

	NSString *kvPath = [self mappedKVPathWithID:mmapID];
	if ([fileManager fileExistsAtPath:kvPath] == NO) {
		// kv file not exist
		return YES;
	}

	NSString *crcPath = [self crcPathWithMappedKVPath:kvPath];
	if ([fileManager fileExistsAtPath:crcPath] == NO) {
		// crc file not exist
		return YES;
	}

	uint32_t crcFile = 0;
	@try {
		NSData *fileData = [NSData dataWithContentsOfFile:crcPath];
		memcpy(&crcFile, fileData.bytes, Fixed32Size);
	} @catch (NSException *exception) {
		return NO;
	}

	size_t actualSize = 0;
	MemoryFile file(kvPath);
	@try {
		actualSize = MiniCodedInputData(&file).readFixed32();
	} @catch (NSException *exception) {
		return NO;
	}

	if (actualSize > file.getFileSize() - Fixed32Size) {
		return NO;
	}

	uint32_t crcDigest = file.crc32(0, Fixed32Size, actualSize);

	return crcFile == crcDigest;
}

@end
