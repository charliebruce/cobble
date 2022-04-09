echo "Building Java code"
javac -source 1.7 -target 1.7 -bootclasspath /Android/Sdk/platforms/android-29/android.jar platforms/android/AndroidBLEImpl.java -d ./build
jar cvf ./build/cobble_android.jar ./build/com/cjb248/cobble/AndroidBLEImpl*.class


export NDK=/android-ndk
export TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
export API=21


# Name must start "lib" or it won't be loaded (at least in Unity, maybe others)
export TARGET=aarch64-linux-android
export AR=$TOOLCHAIN/bin/$TARGET-ar
export AS=$TOOLCHAIN/bin/$TARGET-as
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
export LD=$TOOLCHAIN/bin/$TARGET-ld
export RANLIB=$TOOLCHAIN/bin/$TARGET-ranlib
export STRIP=$TOOLCHAIN/bin/$TARGET-strip
echo "Building native code (arm64)"
$CC \
cobble_events.c \
platforms/android/AndroidBLE.c \
-fPIC -shared -llog -o ./build/libCobble_android_arm64.so


export TARGET=armv7a-linux-androideabi
export AR=$TOOLCHAIN/bin/$TARGET-ar
export AS=$TOOLCHAIN/bin/$TARGET-as
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
export LD=$TOOLCHAIN/bin/$TARGET-ld
export RANLIB=$TOOLCHAIN/bin/$TARGET-ranlib
export STRIP=$TOOLCHAIN/bin/$TARGET-strip
echo "Building native code (armv7a)"
$CC \
cobble_events.c \
platforms/android/AndroidBLE.c \
-fPIC -shared -llog -o ./build/libCobble_android_armv7a.so
