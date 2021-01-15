export ANDROID_NDK_HOME="$HOME/android-ndk-r10c"

cd $HOME/valgrind-official/valgrind

echo $ANDROID_NDK_HOME

export AR="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-ar"
export LD="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-ld"
export CC="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-gcc"
export CXX="$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-g++"
export CPPFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/android-21/arch-arm64" 
export CFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/android-21/arch-arm64" \

./autogen.sh




./configure --prefix="/data/local/tmp/Inst" \
	 --host=aarch64-unknown-linux \
	 --target=aarch64-unknown-linux \
        --with-tmpdir="/sdcard "


make -j4
make -j4 install DESTDIR=`pwd`/Inst
