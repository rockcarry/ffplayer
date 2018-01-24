#!/bin/bash
set -e

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
  git clone -b ffplayer https://github.com/rockcarry/ffmpeg
fi

SYSROOT=$NDK_HOME/platforms/android-19/arch-arm/
CROSS_COMPILE=$NDK_HOME/toolchains/arm-linux-androideabi-4.9/prebuilt/windows/bin/arm-linux-androideabi-

EXTRA_CFLAGS="-DANDROID -DNDEBUG -Os -ffast-math -mfpu=neon-vfpv4 -mfloat-abi=softfp"

cd ffmpeg
./configure \
--pkg-config=pkg-config \
--arch=armv7 \
--cpu=armv7-a \
--target-os=android \
--enable-cross-compile \
--cross-prefix=$CROSS_COMPILE \
--sysroot=$SYSROOT \
--prefix=$PWD/../ffmpeg-android-sdk \
--enable-static \
--enable-small \
--disable-shared \
--disable-symver \
--disable-debug \
--disable-programs \
--disable-doc \
--disable-postproc \
--disable-encoders \
--disable-muxers   \
--disable-filters  \
--disable-swscale-alpha \
--enable-encoder=mjpeg \
--enable-muxer=mjpeg \
--enable-encoder=apng \
--enable-muxer=apng \
--enable-filter=yadif \
--enable-filter=rotate \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-jni \
--enable-mediacodec \
--enable-decoder=h264_mediacodec \
--enable-decoder=hevc_mediacodec \
--enable-decoder=mpeg2_mediacodec \
--enable-decoder=mpeg4_mediacodec \
--enable-decoder=vp8_mediacodec \
--enable-decoder=vp9_mediacodec \
--extra-cflags="$EXTRA_CFLAGS"
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

