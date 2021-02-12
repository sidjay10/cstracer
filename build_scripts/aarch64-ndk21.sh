#!/bin/bash

#########################################################

# This script builds valgrind for aarch64-android
# using ndk21 and clang

# If you get compilation errors, refer to
# arm64_android_clang_patches

#########################################################

#########################################################
#Set these variables accordingly
#########################################################

export ANDROID_NDK_HOME="$HOME/android-ndk-r21d"
VALGRIND_HOME=$HOME/cstracer

#########################################################

#install location on the android device
INSTALL_DIR="/data/local/tmp/Inst"

#########################################################

cd $VALGRIND_HOME 

export TOOLCHAIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/"
export TARGET=aarch64-linux-android

export API=21

export AR="$TOOLCHAIN/bin/$TARGET-ar"
export AS="$TOOLCHAIN/bin/$TARGET-as"
export CC="$TOOLCHAIN/bin/$TARGET$API-clang"
export CXX="$TOOLCHAIN/bin/$TARGET$API-clang++"
export LD="$TOOLCHAIN/bin/$TARGET-ld"
#export RANLIB="$TOOLCHAIN/bin/$TARGET-ranlib"
#export STRIP="$TOOLCHAIN/bin/$TARGET-strip"

export CFLAGS="-DVGC_android_clang=1"
export CPPFLAGS="-DVGC_android_clang=1"

make clean
./autogen.sh

./configure --prefix=$INSTALL_DIR \
	 --host=aarch64-unknown-linux \
	 --target=aarch64-unknown-linux \
        --with-tmpdir="/sdcard "

make clean

make -j4
make -j4 install DESTDIR=`pwd`/Inst_ndk21
