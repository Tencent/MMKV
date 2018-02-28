//
//  PBCoder.h
//  PBCoder
//
//  Created by Guo Ling on 4/14/13.
//  Copyright (c) 2013 Guo Ling. All rights reserved.
//

#ifdef __cplusplus

#import <Foundation/Foundation.h>

@interface PBCoder : NSObject

+(NSData*) encodeDataWithObject:(id)obj;

+(id) decodeObjectOfClass:(Class)cls fromData:(NSData*)oData;

// for NSArray/NSDictionary, etc
// 注意：Dictionary 的 key 必须是 NSString 类型
+(id) decodeContainerOfClass:(Class)cls withValueClass:(Class)valueClass fromData:(NSData*)oData;

@end

#endif
