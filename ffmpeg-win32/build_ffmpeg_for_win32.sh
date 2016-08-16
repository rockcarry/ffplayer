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
--disable-parsers  \
--disable-bsfs     \
--disable-devices  \
--disable-filters  \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-d3d11va \
--enable-dxva2   \
--enable-nvenc   \
--enable-opencl  \
--enable-libmfx  \
--enable-libass  \
--extra-cflags="-I$PWD/../include" \
--extra-ldflags="-L$PWD/../lib"
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

