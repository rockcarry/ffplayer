#!/bin/bash

if [ -d ffmpeg ]; then
  cd ffmpeg
else
  git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg
  cd ffmpeg
fi

sed -i '/check_cflags -Werror=missing-prototypes/d' configure

./configure \
--arch=x86 \
--cpu=i586 \
--target-os=mingw32 \
--enable-cross-compile \
--cross-prefix=i586-mingw32msvc- \
--pkg-config=pkg-config \
--prefix=$PWD/.. \
--enable-static \
--enable-shared \
--enable-small \
--disable-symver \
--disable-debug \
--enable-memalign-hack \
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
--enable-nonfree

make -j8 && make install

cd -

rm -rf ffmpeg lib
