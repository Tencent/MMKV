//
//  flutter-bridge.h
//  mmkv_ios
//
//  Created by lingol on 2024/9/12.
//

#ifndef flutter_bridge_h
#define flutter_bridge_h

#import <MMKV/MMKV.h>

enum MMKVErrorType : int {
    MMKVCRCCheckFail = 0,
    MMKVFileLength,
};

using ErrorCallback_t = uint32_t (*)(const char *mmapID, uint32_t errorType);
using ContenctChangeCallback_t = void (*)(const char *mmapID);
using LogCallback_t = void (*)(uint32_t level, const char *file, int32_t line, const char *funcname, const char *message);

@interface MyMMKVHandler : NSObject<MMKVHandler>

@property (atomic, assign) ErrorCallback_t errorCallback;
@property (atomic, assign) LogCallback_t logCallback;
@property (atomic, assign) ContenctChangeCallback_t contenctChangeCallback;

+(MyMMKVHandler *) getHandler;

@end

#endif /* flutter_bridge_h */
