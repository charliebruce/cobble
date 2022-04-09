mkdir -p build

# Test executable (default architecture)
clang -framework Foundation -framework CoreBluetooth \
cobble_events.c \
cobble_scan_example.c \
platforms/apple/AppleBLE.m \
-o build/cobble_mac

# macOS Library (Apple Silicon and x86_64)
# Verify the presence of both using lipo -info
clang -arch arm64 -arch x86_64 -framework Foundation -framework CoreBluetooth -shared -fpic \
cobble_events.c \
platforms/apple/AppleBLE.m \
-o build/cobble_mac.dylib

# iOS Library (arm64 and armv7)
clang -arch arm64 -arch armv7 -O3 -mios-version-min=9.0 -fembed-bitcode -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk -r -framework CoreBluetooth -framework Foundation  /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk/usr/lib/libc.tbd /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk/usr/lib/libm.tbd \
cobble_events.c \
platforms/apple/AppleBLE.m \
-I ./platforms/apple \
-o build/cobble_ios.a
