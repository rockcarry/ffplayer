#!/bin/bash
set -e

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
  git clone -b ffplayer https://github.com/rockcarry/ffmpeg
fi

CROSS_COMPILE=i686-w64-mingw32-

cd ffmpeg
./configure \
--pkg-config=pkg-config \
--arch=x86 \
--target-os=mingw32 \
--enable-cross-compile \
--cross-prefix=$CROSS_COMPILE \
--prefix=$PWD/../ffmpeg-win32-sdk \
--enable-static \
--enable-shared \
--enable-small \
--enable-memalign-hack \
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
--enable-dxva2 \
--enable-d3d11va \
--enable-libmfx
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

