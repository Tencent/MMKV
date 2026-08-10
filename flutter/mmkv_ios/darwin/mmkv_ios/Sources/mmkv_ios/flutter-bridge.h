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

// SwiftPM packages native targets as static archives. Dart resolves the FFI
// entry points with dlsym(), so those lookups don't create linker references
// that would extract this translation unit from the archive. MMKVPlugin calls
// this anchor to keep the bridge, and therefore MMKVCore, in the final binary.
extern "C" void mmkvEnsureCoreLinked(void);

@interface MyMMKVHandler : NSObject<MMKVHandler>

@property (atomic, assign) ErrorCallback_t errorCallback;
@property (atomic, assign) LogCallback_t logCallback;
@property (atomic, assign) ContenctChangeCallback_t contenctChangeCallback;
@property (atomic, assign) ContenctChangeCallback_t contentLoadedCallback;

+(MyMMKVHandler *) getHandler;

@end

#endif /* flutter_bridge_h */
