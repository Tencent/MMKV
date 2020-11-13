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
    if ([@"getPlatformVersion" isEqualToString:call.method]) {
        result([@"iOS " stringByAppendingString:[[UIDevice currentDevice] systemVersion]]);
    } else if ([@"initializeMMKV" isEqualToString:call.method]) {
        NSString *rootDir = [call.arguments objectForKey:@"rootDir"];
        NSNumber *logLevel = [call.arguments objectForKey:@"logLevel"];
        NSString *groupDir = [call.arguments objectForKey:@"groupDir"];
        if (groupDir.length > 0) {
            [MMKV initializeMMKV:rootDir groupDir:groupDir logLevel:logLevel.intValue];
        } else {
            [MMKV initializeMMKV:rootDir logLevel:logLevel.intValue];
        }
    } else {
        result(FlutterMethodNotImplemented);
    }
}

@end
