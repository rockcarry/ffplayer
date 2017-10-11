#!/bin/bash
set -e

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
git clone git://source.ffmpeg.org/ffmpeg.git
fi

EXTRA_CFLAGS="-DANDROID -DNDEBUG -Os -mfpu=neon-vfpv4 -mfloat-abi=softfp"

cd ffmpeg
./configure \
--arch=arm \
--cpu=armv7-a \
--target-os=linux \
--enable-cross-compile \
--cross-prefix=arm-linux-androideabi- \
--prefix=$PWD/.. \
--enable-static \
--enable-small \
--disable-shared \
--disable-swscale-alpha \
--disable-symver \
--disable-debug \
--disable-programs \
--disable-doc \
--disable-postproc \
--disable-encoders \
--disable-muxers   \
--disable-filters  \
--enable-encoder=mjpeg \
--enable-muxer=mjpeg \
--enable-encoder=apng \
--enable-muxer=apng \
--enable-filter=yadif \
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

