#!/bin/bash

if [ -d ffmpeg ]; then
  cd ffmpeg
else
  git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg
  cd ffmpeg
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

./configure \
--arch=arm \
--target-os=linux \
--enable-cross-compile \
--cross-prefix=arm-linux-androideabi- \
--prefix=$PWD/.. \
--enable-static \
--enable-shared \
--enable-small \
--disable-symver \
--disable-debug \
--disable-programs \
--disable-doc \
--disable-avdevice \
--disable-avfilter \
--disable-postproc \
--disable-encoders \
--disable-devices  \
--disable-filters  \
--disable-muxers \
--disable-swscale-alpha \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--extra-cflags="$CFLAGS $EXTRA_CFLAGS" \
--extra-ldflags="$EXTRA_LDFLAGS"

make -j8 && make install

cd -

rm -rf ffmpeg
