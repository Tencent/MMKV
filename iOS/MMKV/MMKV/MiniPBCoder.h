//
//  MiniPBCoder.h
//  PBCoder
//
//  Created by Guo Ling on 4/14/13.
//  Copyright (c) 2013 Guo Ling. All rights reserved.
//

#ifdef __cplusplus

#import <Foundation/Foundation.h>

@interface MiniPBCoder : NSObject

+ (NSData *)encodeDataWithObject:(id)obj;

+ (id)decodeObjectOfClass:(Class)cls fromData:(NSData *)oData;

// for NSDictionary
// note: NSDictionary's key must be NSString
+ (id)decodeContainerOfClass:(Class)cls withValueClass:(Class)valueClass fromData:(NSData *)oData;

@end

#endif
