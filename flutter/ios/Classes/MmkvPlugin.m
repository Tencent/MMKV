#import "MmkvPlugin.h"
#import <MMKV/MMKV.h>

@implementation MmkvPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar> *)registrar {
    FlutterMethodChannel *channel = [FlutterMethodChannel
        methodChannelWithName:@"mmkv"
              binaryMessenger:[registrar messenger]];
    MmkvPlugin *instance = [[MmkvPlugin alloc] init];
    [registrar addMethodCallDelegate:instance channel:channel];
}

- (void)handleMethodCall:(FlutterMethodCall *)call result:(FlutterResult)result {
    if ([@"initializeMMKV" isEqualToString:call.method]) {
        NSString *rootDir = [call.arguments objectForKey:@"rootDir"];
        NSNumber *logLevel = [call.arguments objectForKey:@"logLevel"];
        NSString *groupDir = [call.arguments objectForKey:@"groupDir"];
        NSString *ret = nil;
        if (groupDir.length > 0) {
            ret = [MMKV initializeMMKV:rootDir groupDir:groupDir logLevel:logLevel.intValue];
        } else {
            ret = [MMKV initializeMMKV:rootDir logLevel:logLevel.intValue];
        }
        result(ret);
    } else {
        result(FlutterMethodNotImplemented);
    }
}

@end
