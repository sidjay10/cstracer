#!/bin/bash

#########################################################

# This script builds valgrind for amd64-android
# using ndk10 and gcc

#########################################################

#########################################################
#Set these variables accordingly
#########################################################

export ANDROID_NDK_HOME="$HOME/android-ndk-r10c"
VALGRIND_HOME=$HOME/cstracer

#########################################################

#install location on the android device
INSTALL_DIR="/data/local/tmp/Inst"

#########################################################

cd $VALGRIND_HOME 

echo $ANDROID_NDK_HOME

export AR="$ANDROID_NDK_HOME/toolchains/x86_64-4.9/prebuilt/linux-x86_64/bin/x86_64-linux-android-ar"
export LD="$ANDROID_NDK_HOME/toolchains/x86_64-4.9/prebuilt/linux-x86_64/bin/x86_64-linux-android-ld"
export CC="$ANDROID_NDK_HOME/toolchains/x86_64-4.9/prebuilt/linux-x86_64/bin/x86_64-linux-android-gcc"
export CXX="$ANDROID_NDK_HOME/toolchains/x86_64-4.9/prebuilt/linux-x86_64/bin/x86_64-linux-android-g++"
export CPPFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/android-21/arch-x86_64" 
export CFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/android-21/arch-x86_64 -fno-pic -DCST_NO_SYMBOLS=1" \


make clean

./autogen.sh


./configure --prefix=$INSTALL_DIR \
	 --host=amd64-android-linux \
	 --target=amd64-android-linux \
        --with-tmpdir="/sdcard"

make clean


make -j4
make -j4 install DESTDIR=`pwd`/Inst-amd64
