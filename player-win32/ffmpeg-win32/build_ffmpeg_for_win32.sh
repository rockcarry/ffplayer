#!/bin/bash
set -e


#++ build x264 ++#
if false; then
if [ ! -d x264 ]; then
  git clone git://git.videolan.org/x264.git
fi
cd x264
./configure \
--enable-strip \
--enable-static \
--enable-shared \
--host=i686-w64-mingw32 \
--cross-prefix=i686-w64-mingw32-
make -j8 && make install
cd -
fi
#-- build x264 --#


#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
  git clone -b ffplayer https://github.com/rockcarry/ffmpeg
fi
cd ffmpeg
./configure \
--pkg-config=pkg-config \
--arch=x86 \
--target-os=mingw32 \
--enable-cross-compile \
--cross-prefix=i686-w64-mingw32- \
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
--enable-encoder=apng \
--enable-encoder=libx264 \
--enable-encoder=aac \
--enable-muxer=mjpeg \
--enable-muxer=apng \
--enable-muxer=mp4 \
--enable-muxer=flv \
--enable-filter=yadif \
--enable-filter=rotate \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-dxva2 \
--enable-d3d11va \
--enable-libmfx \
--enable-libx264
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

