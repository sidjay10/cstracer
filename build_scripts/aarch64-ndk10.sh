#!/bin/bash

#########################################################

# This script builds valgrind for aarch64-android
# using ndk10 and gcc

#########################################################

#########################################################
#Set these variables accordingly
#########################################################

export ANDROID_NDK_HOME="$HOME/android-ndk-r10c"
VALGRIND_HOME=$HOME/cstracer/

#########################################################

#install location on the android device
INSTALL_DIR="/data/local/tmp/Inst"

#########################################################

cd $VALGRIND_HOME 

echo $ANDROID_NDK_HOME

export AR="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-ar"
export LD="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-ld"
export CC="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-gcc"
export CXX="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-g++"
export CPPFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/android-21/arch-arm64" 
export CFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/android-21/arch-arm64" \


make clean

./autogen.sh


./configure --prefix=$INSTALL_DIR \
	 --host=aarch64-unknown-linux \
	 --target=aarch64-unknown-linux \
        --with-tmpdir="/sdcard "

make clean


make -j4
make -j4 install DESTDIR=`pwd`/Inst
