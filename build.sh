#!/bin/sh
# Build libVCTracerMenu.so (armeabi-v7a). Needs the Android NDK (r25+ recommended).
#   export NDK=/path/to/android-ndk-r25c
#   ./build.sh
set -e
: "${NDK:?set NDK to your android-ndk path}"
CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi21-clang++"
"$CXX" -shared -fPIC -O2 -mthumb -fno-exceptions -fno-rtti -fno-math-errno -fvisibility=hidden \
  -nostdlib++ -Wl,-soname,libVCTracerMenu.so -Wl,--no-undefined \
  -o libVCTracerMenu.so VCTracerMenu.cpp -ldl -lm
echo "built libVCTracerMenu.so"
