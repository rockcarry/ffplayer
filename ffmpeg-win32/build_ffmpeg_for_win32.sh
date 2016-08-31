#!/bin/bash
set -e

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
git clone git://source.ffmpeg.org/ffmpeg.git
fi

#+ patch for mingw32 compile error
sed -i '/check_cflags -Werror=missing-prototypes/d' ffmpeg/configure
#- patch for mingw32 compile error

cd ffmpeg
./configure \
--pkg-config=pkg-config \
--arch=x86 \
--target-os=mingw32 \
--enable-cross-compile \
--cross-prefix=i686-w64-mingw32- \
--prefix=$PWD/../ffmpeg-win-sdk \
--enable-static \
--enable-shared \
--enable-small \
--enable-memalign-hack \
--disable-swscale-alpha \
--disable-symver \
--disable-debug \
--disable-programs \
--disable-doc \
--disable-avdevice \
--disable-avfilter \
--disable-postproc \
--disable-encoders \
--disable-muxers   \
--disable-devices  \
--disable-filters  \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-dxva2 \
--enable-d3d11va \
--enable-libmfx \
--extra-ldflags="-static-libgcc -static-libstdc++"
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

