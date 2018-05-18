//
//  ScopedLock.hpp
//  MMKV
//
//  Created by Ling Guo on 02/03/2018.
//  Copyright © 2018 Lingol. All rights reserved.
//

#ifndef ScopedLock_h
#define ScopedLock_h

#import <Foundation/Foundation.h>

class CScopedLock
{
	NSRecursiveLock* m_oLock;
	
public:
	CScopedLock(NSRecursiveLock* oLock) : m_oLock(oLock)
	{
		[m_oLock lock];
	}
	~CScopedLock()
	{
		[m_oLock unlock];
		m_oLock = nil;
	}
};

#endif /* ScopedLock_h */
