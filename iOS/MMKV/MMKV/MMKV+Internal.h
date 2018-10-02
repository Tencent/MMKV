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

#ifndef MMKV_Internal_h
#define MMKV_Internal_h

#import "AESCrypt.h"
#import "DynamicBitset.h"
#import "MMKV.h"
#import "MMKVLog.h"
#import "MiniCodedInputData.h"
#import "MiniCodedOutputData.h"
#import "MiniPBCoder.h"
#import "MiniPBUtility.h"
#import "ScopedLock.hpp"

#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
#import <UIKit/UIKit.h>
#endif

#import <algorithm>
#import <sys/mman.h>
#import <sys/stat.h>
#import <unistd.h>

constexpr uint32_t Fixed32Size = pbFixed32Size(0);
constexpr uint32_t ItemSizeHolder = 0x00ffffff, ItemSizeHolderSize = 4;

@interface MMKV () {
	NSRecursiveLock *m_lock;
	KVItemsWrap m_kvItemsWrap;

	NSString *m_path;
	NSString *m_crcPath;
	NSString *m_mmapID;
	size_t m_size;
	size_t m_actualSize;
	MiniCodedOutputData *m_output;
    // TODO: encryption
    AESCrypt *m_cryptor;

	MemoryFile *m_memoryFile;
	std::shared_ptr<MemoryFile::Segment> m_headSegmemt;

	BOOL m_isInBackground;
	BOOL m_needLoadFromFile;

	uint32_t m_crcDigest;
	int m_crcFd;
	char *m_crcPtr;
}

- (void)checkLoadData;
- (BOOL)isFileValid;

- (BOOL)fullWriteback;
- (BOOL)ensureMemorySize:(size_t)newSize;

- (BOOL)writeAcutalSize:(size_t)actualSize;
- (void)writeBackCRCDigest;

@end

#endif /* MMKV_Internal_h */
