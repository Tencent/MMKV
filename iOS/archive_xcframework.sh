#!/bin/bash

rm -rf archives
xcodebuild archive \
-workspace MMKV.xcworkspace \
-scheme MMKV \
-configuration Release \
-destination "generic/platform=iOS" \
-archivePath "archives/MMKV-iOS" \
SKIP_INSTALL=NO \
BUILD_LIBRARY_FOR_DISTRIBUTION=YES

xcodebuild archive \
-workspace MMKV.xcworkspace \
-scheme MMKV \
-configuration Release \
-destination "generic/platform=iOS Simulator" \
-archivePath "archives/MMKV-iOS-Simulator" \
SKIP_INSTALL=NO \
BUILD_LIBRARY_FOR_DISTRIBUTION=YES

xcodebuild archive \
-workspace MMKV.xcworkspace \
-scheme MMKV \
-configuration Release \
-destination "generic/platform=macOS" \
-archivePath "archives/MMKV-macOS" \
SKIP_INSTALL=NO \
BUILD_LIBRARY_FOR_DISTRIBUTION=YES

xcodebuild \
-create-xcframework \
-framework archives/MMKV-iOS.xcarchive/Products/Library/Frameworks/MMKV.framework \
-debug-symbols $PWD/archives/MMKV-iOS.xcarchive/dSYMs/MMKV.framework.dSYM \
-debug-symbols $PWD/archives/MMKV-iOS.xcarchive/BCSymbolMaps/0909AADB-3F01-3418-B7F2-E2B9FB6E315F.bcsymbolmap \
-debug-symbols $PWD/archives/MMKV-iOS.xcarchive/BCSymbolMaps/441383DA-0478-34F4-9A38-F2521EB5B045.bcsymbolmap \
-framework archives/MMKV-iOS-Simulator.xcarchive/Products/Library/Frameworks/MMKV.framework \
-debug-symbols $PWD/archives/MMKV-iOS-Simulator.xcarchive/dSYMs/MMKV.framework.dSYM \
-framework archives/MMKV-macOS.xcarchive/Products/Library/Frameworks/MMKV.framework \
-debug-symbols $PWD/archives/MMKV-macOS.xcarchive/dSYMs/MMKV.framework.dSYM \
-output archives/MMKV.xcframework