#!/bin/bash
set -e

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
git clone git://source.ffmpeg.org/ffmpeg.git
fi

CFLAGS="-O3 -Wall -mthumb -pipe -fpic -fasm \
  -finline-limit=300 -ffast-math \
  -fstrict-aliasing -Werror=strict-aliasing \
  -fmodulo-sched -fmodulo-sched-allow-regmoves \
  -Wno-psabi -Wa,--noexecstack \
  -D__ARM_ARCH_5__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5TE__ \
  -DANDROID -DNDEBUG"
EXTRA_CFLAGS="-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp"
EXTRA_LDFLAGS="-Wl,--fix-cortex-a8"

cd ffmpeg
./configure \
--arch=arm \
--target-os=linux \
--enable-cross-compile \
--cross-prefix=arm-linux-androideabi- \
--prefix=$PWD/.. \
--enable-static \
--enable-shared \
--enable-small \
--disable-swscale-alpha \
--disable-symver \
--disable-debug \
--disable-programs \
--disable-doc \
--disable-avfilter \
--disable-postproc \
--disable-encoders \
--disable-filters  \
--enable-muxers    \
--enable-encoder=mjpeg \
--enable-muxer=mjpeg \
--enable-encoder=apng \
--enable-muxer=apng \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-jni \
--enable-mediacodec \
--enable-decoder=h264_mediacodec \
--extra-cflags="$CFLAGS $EXTRA_CFLAGS" \
--extra-ldflags="$EXTRA_LDFLAGS"
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

