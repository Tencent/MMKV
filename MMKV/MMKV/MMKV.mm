//
//  MMKV.mm
//  MMKV
//
//  Created by Ling Guo on 5/18/15.
//  Copyright (c) 2015 WXG. All rights reserved.
//

#import "MMKV.h"
#import <UIKit/UIKit.h>
#import "MiniPBCoder.h"
#import "PBUtility.h"
#import "CodedInputData.h"
#import "CodedOutputData.h"
#import <sys/mman.h>
#import <sys/stat.h>
#import <unistd.h>
#import <zlib.h>
#import <algorithm>
#import "ScopedLock.hpp"

#define MMKVError(format, ...)	NSLog(format, ##__VA_ARGS__)
#define MMKVWarning(format, ...)	NSLog(format, ##__VA_ARGS__)
#define MMKVInfo(format, ...)		NSLog(format, ##__VA_ARGS__)

static NSMutableDictionary* g_instanceDic;
static NSRecursiveLock* g_instanceLock;
const int DEFAULT_MMAP_SIZE = getpagesize();

#define DEFAULT_MMAP_ID @"mmkv.default"

@implementation MMKV {
	NSRecursiveLock* m_lock;
	NSMutableDictionary* m_dic;
	NSString* m_path;
	NSString* m_crcPath;
	NSString* m_mmapID;
	int m_fd;
	char* m_ptr;
	size_t m_size;
	size_t m_actualSize;
	CodedOutputData* m_output;
	
	BOOL m_isInBackground;
	BOOL m_needLoadFromFile;
	
	uint32_t m_crcDigest;
	int m_crcFd;
	char* m_crcPtr;
}

#pragma mark - init

+(void)initialize {
	if (self == MMKV.class) {
		static dispatch_once_t onceToken;
		dispatch_once(&onceToken, ^{
			g_instanceDic = [NSMutableDictionary dictionary];
			g_instanceLock = [[NSRecursiveLock alloc] init];
			
			MMKVInfo(@"pagesize:%d", DEFAULT_MMAP_SIZE);
		});
	}
}

// a generic purpose instance
+(instancetype)defaultMMKV {
	return [MMKV mmkvWithID:DEFAULT_MMAP_ID];
}

// any unique ID (com.tencent.xin.pay, etc)
+(instancetype)mmkvWithID:(NSString*)mmapID {
	if (mmapID.length <= 0) {
		return nil;
	}
	CScopedLock lock(g_instanceLock);
	
	MMKV* kv = [g_instanceDic objectForKey:mmapID];
	if (kv == nil) {
		kv = [[MMKV alloc] initWithMMapID:mmapID];
		[g_instanceDic setObject:kv forKey:mmapID];
	}
	return kv;
}

-(instancetype)initWithMMapID:(NSString*)mmapID {
	if (self = [super init]) {
		m_lock = [[NSRecursiveLock alloc] init];
		
		m_mmapID = mmapID;
		
		m_path = [MMKV mappedKVPathWithID:m_mmapID];
		if(![MMKV FileExist:m_path]) {
			[MMKV CreateFile:m_path];
		}
		m_crcPath = [MMKV crcPathWithMappedKVPath:m_path];
		
		[self loadFromFile];
		
		auto appState = [UIApplication sharedApplication].applicationState;
		if (appState != UIApplicationStateActive) {
			m_isInBackground = YES;
		} else {
			m_isInBackground = NO;
		}
		MMKVInfo(@"m_isInBackground:%d, appState:%ld", m_isInBackground, (long)appState);
		
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onMemoryWarning) name:UIApplicationDidReceiveMemoryWarningNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didEnterBackground) name:UIApplicationDidEnterBackgroundNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didBecomeActive) name:UIApplicationDidBecomeActiveNotification object:nil];
	}
	return self;
}

-(void)dealloc {
	CScopedLock lock(m_lock);
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	if (m_ptr != MAP_FAILED && m_ptr != NULL) {
		munmap(m_ptr, m_size);
		m_ptr = NULL;
	}
	if (m_fd > 0) {
		close(m_fd);
		m_fd = -1;
	}
	if (m_output) {
		delete m_output;
		m_output = NULL;
	}
	
	if (m_crcPtr != NULL && m_crcPtr != MAP_FAILED) {
		munmap(m_crcPtr, computeFixed32Size(0));
		m_crcPtr = NULL;
	}
	if (m_crcFd > 0) {
		close(m_crcFd);
		m_crcFd = -1;
	}
}

#pragma mark - Application state

-(void)onMemoryWarning {
	CScopedLock lock(m_lock);
	
	MMKVInfo(@"cleaning on memory warning %@", m_mmapID);
	
	if (m_needLoadFromFile) {
		MMKVInfo(@"ignore %@", m_mmapID);
		return;
	}
	m_needLoadFromFile = YES;
	
	[self clearMemoryState];
}

-(void)didEnterBackground {
	CScopedLock lock(m_lock);
	
	m_isInBackground = YES;
	MMKVInfo(@"m_isInBackground:%d", m_isInBackground);
}

-(void)didBecomeActive {
	CScopedLock lock(m_lock);
	
	m_isInBackground = NO;
	MMKVInfo(@"m_isInBackground:%d", m_isInBackground);
}

#pragma mark - really dirty work

-(void)loadFromFile {
	m_fd = open(m_path.UTF8String, O_RDWR, S_IRWXU);
	if (m_fd <= 0) {
		MMKVError(@"fail to open:%@, %s", m_path, strerror(errno));
	} else {
		m_size = 0;
		struct stat st = {};
		if (fstat(m_fd, &st) != -1) {
			m_size = (size_t)st.st_size;
		}
		// round up to (n * pagesize)
		if (m_size < DEFAULT_MMAP_SIZE || (m_size % DEFAULT_MMAP_SIZE != 0)) {
			m_size = ((m_size / DEFAULT_MMAP_SIZE) + 1 ) * DEFAULT_MMAP_SIZE;
			if (ftruncate(m_fd, m_size) != 0) {
				MMKVError(@"fail to truncate [%@] to size %zu, %s", m_mmapID, m_size, strerror(errno));
				m_size = (size_t)st.st_size;
			}
		}
		m_ptr = (char*)mmap(NULL, m_size, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, 0);
		if (m_ptr == MAP_FAILED) {
			MMKVError(@"fail to mmap [%@], %s", m_mmapID, strerror(errno));
		} else {
			const int offset = computeFixed32Size(0);
			NSData* lenBuffer = [NSData dataWithBytesNoCopy:m_ptr length:offset freeWhenDone:NO];
			@try {
				m_actualSize = CodedInputData(lenBuffer).readFixed32();
			} @catch(NSException *exception) {
				MMKVError(@"%@", exception);
			}
			MMKVInfo(@"loading [%@] with %zu size in total, file size is %zu", m_mmapID, m_actualSize, m_size);
			if (m_actualSize > 0) {
				if (m_actualSize < m_size && m_actualSize+offset <= m_size) {
					if ([self checkFileCRCValid] == YES) {
						NSData* inputBuffer = [NSData dataWithBytesNoCopy:m_ptr+offset length:m_actualSize freeWhenDone:NO];
						m_dic = [MiniPBCoder decodeContainerOfClass:NSMutableDictionary.class withValueClass:NSData.class fromData:inputBuffer];
						m_output = new CodedOutputData(m_ptr+offset+m_actualSize, m_size-offset-m_actualSize);
					} else {
						[self writeAcutalSize:0];
						m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
						[self recaculateCRCDigest];
					}
				} else {
					MMKVError(@"load [%@] error: %zu size in total, file size is %zu", m_mmapID, m_actualSize, m_size);
					[self writeAcutalSize:0];
					m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
					[self recaculateCRCDigest];
				}
			} else {
				m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
				[self recaculateCRCDigest];
			}
			MMKVInfo(@"loaded [%@] with %zu values", m_mmapID, (unsigned long)m_dic.count);
		}
	}
	if (m_dic == nil) {
		m_dic = [NSMutableDictionary dictionary];
	}
	
	if (![self isFileValid]) {
		MMKVWarning(@"[%@] file not valid", m_mmapID);
	}
	
	[MMKV tryResetFileProtection:m_path];
	[MMKV tryResetFileProtection:m_crcPath];
	m_needLoadFromFile = NO;
}

-(void)checkLoadData {
	//	CScopedLock lock(m_lock);
	
	if (m_needLoadFromFile == NO) {
		return;
	}
	m_needLoadFromFile = NO;
	[self loadFromFile];
}

-(void)clearAll {
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
	
	if (m_output != NULL) {
		delete m_output;
	}
	m_output = NULL;
	
	if (m_ptr != NULL && m_ptr != MAP_FAILED) {
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
	m_ptr = NULL;
	
	if (m_fd > 0) {
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
	m_fd = 0;
	m_size = 0;
	m_actualSize = 0;
	m_crcDigest = 0;
	
	[self loadFromFile];
}

-(void)clearMemoryState {
	[m_dic removeAllObjects];
	
	if (m_output != NULL) {
		delete m_output;
	}
	m_output = NULL;
	
	if (m_ptr != NULL && m_ptr != MAP_FAILED) {
		if (munmap(m_ptr, m_size) != 0) {
			MMKVError(@"fail to munmap [%@], %s", m_mmapID, strerror(errno));
		}
	}
	m_ptr = NULL;
	
	if (m_fd > 0) {
		if (close(m_fd) != 0) {
			MMKVError(@"fail to close [%@], %s", m_mmapID, strerror(errno));
		}
	}
	m_fd = 0;
	m_size = 0;
	m_actualSize = 0;
}

-(BOOL)protectFromBackgroundWritting:(size_t)size writeBlock:(void (^)(CodedOutputData* output))block {
	@try {
		if (m_isInBackground) {
			static const int offset = computeFixed32Size(0);
			static const int pagesize = getpagesize();
			size_t realOffset = offset + m_actualSize - size;
			size_t pageOffset = (realOffset / pagesize) * pagesize;
			size_t pointerOffset = realOffset - pageOffset;
			size_t mmapSize = offset + m_actualSize - pageOffset;
			char* ptr = m_ptr+pageOffset;
			if (mlock(ptr, mmapSize) != 0) {
				MMKVError(@"fail to mlock [%@], %s", m_mmapID, strerror(errno));
				// just fail on this condition, otherwise app will crash anyway
				//block(m_output);
				return NO;
			} else {
				CodedOutputData output(ptr + pointerOffset, size);
				block(&output);
				m_output->seek(size);
			}
			munlock(ptr, mmapSize);
		} else {
			block(m_output);
		}
	} @catch(NSException *exception) {
		  MMKVError(@"%@", exception);
		  return NO;
	}
	
	return YES;
}

// since we use append mode, when -[setData: forKey:] many times, space may not be enough
// try a full rewrite to make space
-(BOOL)ensureMemorySize:(size_t)newSize {
	[self checkLoadData];
	
	if (![self isFileValid]) {
		MMKVWarning(@"[%@] file not valid", m_mmapID);
		return NO;
	}
	
	if (newSize >= m_output->spaceLeft()) {
		// try a full rewrite to make space
		static const int offset = computeFixed32Size(0);
		NSData* data = [MiniPBCoder encodeDataWithObject:m_dic];
		size_t lenNeeded = data.length + offset + newSize;
		size_t futureUsage = newSize * std::max<size_t>(8, (m_dic.count+1)/2);
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
			m_ptr = (char*)mmap(m_ptr, m_size, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, 0);
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
			m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
			m_output->seek(m_actualSize);
		}
		
		if ([self writeAcutalSize:data.length] == NO) {
			return NO;
		}
		
		delete m_output;
		m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
		BOOL ret = [self protectFromBackgroundWritting:m_actualSize writeBlock:^(CodedOutputData *output) {
			output->writeRawData(data);
		}];
		if (ret) {
			[self recaculateCRCDigest];
		}
		return ret;
	}
	return YES;
}

-(BOOL)writeAcutalSize:(size_t)actualSize {
	assert(m_ptr != 0);
	assert(m_ptr != MAP_FAILED);
	
	char* actualSizePtr = m_ptr;
	char* tmpPtr = NULL;
	static const int offset = computeFixed32Size(0);
	
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
		CodedOutputData output(actualSizePtr, offset);
		output.writeFixed32((int32_t)actualSize);
	} @catch(NSException *exception) {
		MMKVError(@"%@", exception);
	}
	m_actualSize = actualSize;
	
	if (tmpPtr != NULL && tmpPtr != MAP_FAILED) {
		munlock(tmpPtr, offset);
	}
	return YES;
}

-(BOOL)setData:(NSData*)data forKey:(NSString*)key {
	if (data.length <= 0 || key.length <= 0) {
		return NO;
	}
	size_t keyLength = [key lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	size_t size = keyLength + computeRawVarint32Size((int32_t)keyLength);		// size needed to encode the key
	size += data.length + computeRawVarint32Size((int32_t)data.length);			// size needed to encode the value
	
	CScopedLock lock(m_lock);
	BOOL hasEnoughSize = [self ensureMemorySize:size];
	
	[m_dic setObject:data forKey:key];
	
	if (hasEnoughSize == NO || [self isFileValid] == NO) {
		return NO;
	}
	if (m_actualSize == 0) {
		NSData* allData = [MiniPBCoder encodeDataWithObject:m_dic];
		if (allData.length > 0) {
			BOOL ret = [self writeAcutalSize:allData.length];
			if (ret) {
				ret = [self protectFromBackgroundWritting:m_actualSize writeBlock:^(CodedOutputData *output) {
					output->writeRawData(allData);		// note: don't write size of data
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
			static const int offset = computeFixed32Size(0);
			ret = [self protectFromBackgroundWritting:size writeBlock:^(CodedOutputData *output) {
				output->writeString(key);
				output->writeData(data);				// note: write size of data
			}];
			if (ret) {
				[self updateCRCDigest:(const uint8_t*)m_ptr+offset+m_actualSize-size withSize:size];
			}
		}
		return ret;
	}
}

-(NSData*)getDataForKey:(NSString*)key {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return [m_dic objectForKey:key];
}

-(BOOL)fullWriteback {
	CScopedLock lock(m_lock);
	if (m_needLoadFromFile) {
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
	
	NSData* allData = [MiniPBCoder encodeDataWithObject:m_dic];
	if (allData.length > 0 && [self isFileValid]) {
		int offset = computeFixed32Size(0);
		if (allData.length + offset <= m_size) {
			BOOL ret = [self writeAcutalSize:allData.length];
			if (ret) {
				delete m_output;
				m_output = new CodedOutputData(m_ptr+offset, m_size-offset);
				ret = [self protectFromBackgroundWritting:m_actualSize writeBlock:^(CodedOutputData *output) {
					output->writeRawData(allData);		// note: don't write size of data
				}];
				if (ret) {
					[self recaculateCRCDigest];
				}
			}
			return ret;
		} else {
			// ensureMemorySize will extend file & full rewrite, no need to write back again
			return [self ensureMemorySize:allData.length+offset-m_size];
		}
	}
	return NO;
}

-(BOOL)isFileValid {
	if (m_fd > 0 && m_size > 0 && m_output != NULL && m_ptr != NULL && m_ptr != MAP_FAILED) {
		return YES;
	}
	//	MMKVWarning(@"[%@] file not valid", m_mmapID);
	return NO;
}

// assuming m_ptr & m_size is set
-(BOOL)checkFileCRCValid {
	if (m_ptr != NULL && m_ptr != MAP_FAILED) {
		int offset = computeFixed32Size(0);
		m_crcDigest = (uint32_t)crc32(0, (const uint8_t*)m_ptr+offset, (uint32_t)m_actualSize);
		
		// for backwark compatibility
		if ([MMKV FileExist:m_crcPath] == NO) {
			MMKVInfo(@"crc32 file not found:%@", m_crcPath);
			return YES;
		}
		NSData* oData = [NSData dataWithContentsOfFile:m_crcPath];
		uint32_t crc32 = 0;
		@try {
			CodedInputData input(oData);
			crc32 = input.readFixed32();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
		if (m_crcDigest == crc32) {
			return YES;
		}
		MMKVError(@"check crc [%@] fail, crc32:%u, m_crcDigest:%u", m_mmapID, crc32, m_crcDigest);
	}
	return NO;
}

-(void)recaculateCRCDigest {
	if (m_ptr != NULL && m_ptr != MAP_FAILED) {
		m_crcDigest = 0;
		int offset = computeFixed32Size(0);
		[self updateCRCDigest:(const uint8_t*)m_ptr+offset withSize:m_actualSize];
	}
}

-(void)updateCRCDigest:(const uint8_t*)ptr withSize:(size_t)length {
	if (ptr == NULL) {
		return;
	}
	m_crcDigest = (uint32_t)crc32(m_crcDigest, ptr, (uint32_t)length);
	
	if (m_crcPtr == NULL || m_crcPtr == MAP_FAILED) {
		[self prepareCRCFile];
	}
	if (m_crcPtr == NULL || m_crcPtr == MAP_FAILED) {
		return;
	}
	
	static const size_t bufferLength = computeFixed32Size(0);
	if (m_isInBackground) {
		if (mlock(m_crcPtr, bufferLength) != 0) {
			MMKVError(@"fail to mlock crc [%@]-%p, %d:%s", m_mmapID, m_crcPtr, errno, strerror(errno));
			// just fail on this condition, otherwise app will crash anyway
			return;
		}
	}
	
	@try {
		CodedOutputData output(m_crcPtr, bufferLength);
		output.writeFixed32((int32_t)m_crcDigest);
	} @catch(NSException *exception) {
		MMKVError(@"%@", exception);
	}
	if (m_isInBackground) {
		munlock(m_crcPtr, bufferLength);
	}
}

-(void)prepareCRCFile {
	if (m_crcPtr == NULL || m_crcPtr == MAP_FAILED) {
		if ([MMKV FileExist:m_crcPath] == NO) {
			[MMKV CreateFile:m_crcPath];
		}
		m_crcFd = open(m_crcPath.UTF8String, O_RDWR, S_IRWXU);
		if (m_crcFd <= 0) {
			MMKVError(@"fail to open:%@, %s", m_crcPath, strerror(errno));
			[MMKV RemoveFile:m_crcPath];
		} else {
			size_t size = 0;
			struct stat st = {};
			if (fstat(m_crcFd, &st) != -1) {
				size = (size_t)st.st_size;
			}
			int fileLegth = DEFAULT_MMAP_SIZE;
			if (size < fileLegth) {
				size = fileLegth;
				if (ftruncate(m_crcFd, size) != 0) {
					MMKVError(@"fail to truncate [%@] to size %zu, %s", m_crcPath, size, strerror(errno));
					close(m_crcFd);
					m_crcFd = -1;
					[MMKV RemoveFile:m_crcPath];
					return;
				}
			}
			m_crcPtr = (char*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, m_crcFd, 0);
			if (m_crcPtr == MAP_FAILED) {
				MMKVError(@"fail to mmap [%@], %s", m_crcPath, strerror(errno));
				close(m_crcFd);
				m_crcFd = -1;
			}
		}
	}
}

#pragma mark - set & get

-(BOOL)setObject:(id)obj forKey:(NSString*)key {
	if (obj == nil || key.length <= 0) {
		return FALSE;
	}
	NSData* data = [MiniPBCoder encodeDataWithObject:obj];
	return [self setData:data forKey:key];
}

-(BOOL)setBool:(BOOL)value forKey:(NSString*)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = computeBoolSize(value);
	NSMutableData* data = [NSMutableData dataWithLength:size];
	CodedOutputData output(data);
	output.writeBool(value);
	
	return [self setData:data forKey:key];
}

-(BOOL)setInt32:(int32_t)value forKey:(NSString*)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = computeInt32Size(value);
	NSMutableData* data = [NSMutableData dataWithLength:size];
	CodedOutputData output(data);
	output.writeInt32(value);
	
	return [self setData:data forKey:key];
}

-(BOOL)setUInt32:(uint32_t)value forKey:(NSString*)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = computeUInt32Size(value);
	NSMutableData* data = [NSMutableData dataWithLength:size];
	CodedOutputData output(data);
	output.writeUInt32(value);
	
	return [self setData:data forKey:key];
}

-(BOOL)setInt64:(int64_t)value forKey:(NSString*)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = computeInt64Size(value);
	NSMutableData* data = [NSMutableData dataWithLength:size];
	CodedOutputData output(data);
	output.writeInt64(value);
	
	return [self setData:data forKey:key];
}

-(BOOL)setUInt64:(uint64_t)value forKey:(NSString*)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = computeUInt64Size(value);
	NSMutableData* data = [NSMutableData dataWithLength:size];
	CodedOutputData output(data);
	output.writeUInt64(value);
	
	return [self setData:data forKey:key];
}

-(BOOL)setFloat:(float)value forKey:(NSString*)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = computeFloatSize(value);
	NSMutableData* data = [NSMutableData dataWithLength:size];
	CodedOutputData output(data);
	output.writeFloat(value);
	
	return [self setData:data forKey:key];
}

-(BOOL)setDouble:(double)value forKey:(NSString*)key {
	if (key.length <= 0) {
		return FALSE;
	}
	size_t size = computeDoubleSize(value);
	NSMutableData* data = [NSMutableData dataWithLength:size];
	CodedOutputData output(data);
	output.writeDouble(value);
	
	return [self setData:data forKey:key];
}

-(id)getObjectOfClass:(Class)cls forKey:(NSString*)key {
	if (key.length <= 0) {
		return nil;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		return [MiniPBCoder decodeObjectOfClass:cls fromData:data];
	}
	return nil;
}

-(bool)getBoolForKey:(NSString*)key {
	return [self getBoolForKey:key defaultValue:FALSE];
}
-(bool)getBoolForKey:(NSString *)key defaultValue:(bool)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		@try {
			CodedInputData input(data);
			return input.readBool();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

-(int32_t)getInt32ForKey:(NSString*)key {
	return [self getInt32ForKey:key defaultValue:0];
}
-(int32_t)getInt32ForKey:(NSString *)key defaultValue:(int32_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		@try {
			CodedInputData input(data);
			return input.readInt32();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

-(uint32_t)getUInt32ForKey:(NSString*)key {
	return [self getUInt32ForKey:key defaultValue:0];
}
-(uint32_t)getUInt32ForKey:(NSString *)key defaultValue:(uint32_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		@try {
			CodedInputData input(data);
			return input.readUInt32();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

-(int64_t)getInt64ForKey:(NSString*)key {
	return [self getInt64ForKey:key defaultValue:0];
}
-(int64_t)getInt64ForKey:(NSString *)key defaultValue:(int64_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		@try {
			CodedInputData input(data);
			return input.readInt64();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

-(uint64_t)getUInt64ForKey:(NSString*)key {
	return [self getUInt64ForKey:key defaultValue:0];
}
-(uint64_t)getUInt64ForKey:(NSString *)key defaultValue:(uint64_t)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		@try {
			CodedInputData input(data);
			return input.readUInt64();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

-(float)getFloatForKey:(NSString*)key {
	return [self getFloatForKey:key defaultValue:0];
}
-(float)getFloatForKey:(NSString*)key defaultValue:(float)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		@try {
			CodedInputData input(data);
			return input.readFloat();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

-(double)getDoubleForKey:(NSString*)key {
	return [self getDoubleForKey:key defaultValue:0];
}
-(double)getDoubleForKey:(NSString*)key defaultValue:(double)defaultValue {
	if (key.length <= 0) {
		return defaultValue;
	}
	NSData* data = [self getDataForKey:key];
	if (data.length > 0) {
		@try {
			CodedInputData input(data);
			return input.readDouble();
		} @catch(NSException *exception) {
			MMKVError(@"%@", exception);
		}
	}
	return defaultValue;
}

#pragma mark - enumerate

-(BOOL)containsKey:(NSString *)key {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return ([m_dic objectForKey:key] != nil);
}

-(size_t)count {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return m_dic.count;
}

-(size_t)totalSize {
	CScopedLock lock(m_lock);
	[self checkLoadData];
	return m_size;
}

-(void)enumerateKeys:(void (^)(NSString* key, BOOL *stop))block {
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

// Expensive! use -[MMKV removeValuesForKeys:] for more than one key
-(void)removeValueForKey:(NSString*)key {
	if (key.length <= 0) {
		return;
	}
	CScopedLock lock(m_lock);
	[self checkLoadData];
	[m_dic removeObjectForKey:key];
	
	[self fullWriteback];
}

-(void)removeValuesForKeys:(NSArray*)arrKeys {
	if (arrKeys.count <= 0) {
		return;
	}
	CScopedLock lock(m_lock);
	[self checkLoadData];
	[m_dic removeObjectsForKeys:arrKeys];
	
	MMKVInfo(@"remove [%@] %lu keys, %lu remain", m_mmapID, (unsigned long)arrKeys.count, (unsigned long)m_dic.count);
	
	[self fullWriteback];
}

#pragma mark - borning stuff

-(void)sync {
	CScopedLock lock(m_lock);
	if (m_needLoadFromFile || ![self isFileValid]) {
		return;
	}
	if (msync(m_ptr, m_size, MS_SYNC) != 0) {
		MMKVError(@"fail to msync [%@]:%s", m_mmapID, strerror(errno));
	}
}

+(NSString*)mappedKVPathWithID:(NSString*)mmapID
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString* nsLibraryPath = (NSString*)[paths firstObject];
	if ([nsLibraryPath length] > 0) {
		return [nsLibraryPath stringByAppendingFormat:@"/mmkv/%@", mmapID];
	} else {
		return @"";
	}
}

+(NSString*)crcPathWithMappedKVPath:(NSString*)kvPath
{
	return [kvPath stringByAppendingString:@".crc"];
}

+(BOOL)isFileValid:(NSString*)mmapID
{
	NSFileManager* fileManager = [NSFileManager defaultManager];
	
	NSString* kvPath = [self mappedKVPathWithID:mmapID];
	if ([fileManager fileExistsAtPath:kvPath] == NO) {
		// kv文件不存在，有效
		return YES;
	}
	
	NSString* crcPath = [self crcPathWithMappedKVPath:kvPath];
	if ([fileManager fileExistsAtPath:crcPath] == NO) {
		// crc文件不存在，有效
		return YES;
	}
	
	// 读取crc文件值
	NSData* fileData = [NSData dataWithContentsOfFile:crcPath];
	uint32_t crcFile = 0;
	@try {
		CodedInputData input(fileData);
		crcFile = input.readFixed32();
	} @catch(NSException *exception) {
		// 有异常，无效
		return NO;
	}
	
	// 计算原kv crc值
	const int offset = computeFixed32Size(0);
	size_t actualSize = 0;
	fileData = [NSData dataWithContentsOfFile:kvPath];
	@try {
		actualSize = CodedInputData(fileData).readFixed32();
	} @catch(NSException *exception) {
		// 有异常，无效
		return NO;
	}
	
	if (actualSize > fileData.length - offset) {
		// 长度异常，无效
		return NO;
	}
	
	uint32_t crcDigest = (uint32_t)crc32(0, (const uint8_t*)fileData.bytes + offset, (uint32_t)actualSize);
	
	return crcFile == crcDigest;
}

#pragma mark - file

+ (BOOL) FileExist:(NSString*)nsFilePath
{
	if([nsFilePath length] == 0) {
		return NO;
	}
	
	struct stat temp;
	return lstat(nsFilePath.UTF8String, &temp) == 0;
}

+ (BOOL) CreateFile:(NSString*)nsFilePath
{
	NSFileManager* oFileMgr = [NSFileManager defaultManager];
	// try create file at once
	NSMutableDictionary* fileAttr = [NSMutableDictionary dictionary];
	[fileAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication forKey:NSFileProtectionKey];
	if([oFileMgr createFileAtPath:nsFilePath contents:nil attributes:fileAttr]) {
		return YES;
	}
	
	// create parent directories
	NSString* nsPath = [nsFilePath stringByDeletingLastPathComponent];
	
	//path is not null && is not '/'
	NSError* err;
	if([nsPath length] > 1 && ![oFileMgr createDirectoryAtPath:nsPath withIntermediateDirectories:YES attributes:nil error:&err]) {
		MMKVError(@"create file path:%@ fail:%@", nsPath, [err localizedDescription]);
		return NO;
	}
	// create file again
	if(![oFileMgr createFileAtPath:nsFilePath contents:nil attributes:fileAttr]) {
		MMKVError(@"create file path:%@ fail.", nsFilePath);
		return NO;
	}
	return YES;
}

+ (BOOL) RemoveFile:(NSString*)nsFilePath
{
	int ret = remove(nsFilePath.UTF8String);
	if (ret != 0) {
		MMKVError(@"remove file failed. filePath=%@, err=%s", nsFilePath, strerror(errno));
		return NO;
	}
	return YES;
}

+(void)tryResetFileProtection:(NSString*)path {
	@autoreleasepool {
		NSDictionary* attr = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL];
		NSString* protection = [attr valueForKey:NSFileProtectionKey];
		MMKVInfo(@"protection on [%@] is %@", path, protection);
		if ([protection isEqualToString:NSFileProtectionCompleteUntilFirstUserAuthentication] == NO) {
			NSMutableDictionary* newAttr = [NSMutableDictionary dictionaryWithDictionary:attr];
			[newAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication forKey:NSFileProtectionKey];
			NSError* err = nil;
			[[NSFileManager defaultManager] setAttributes:newAttr ofItemAtPath:path error:&err];
			if (err != nil) {
				MMKVError(@"fail to set attribute %@ on [%@]: %@", NSFileProtectionCompleteUntilFirstUserAuthentication, path, err);
			}
		}
	}
}

@end

