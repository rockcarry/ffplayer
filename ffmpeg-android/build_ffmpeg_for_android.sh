#!/bin/bash

cd ffmpeg

./configure \
--arch=arm \
--target-os=linux \
--enable-cross-compile \
--cross-prefix=arm-linux-androideabi- \
--pkg-config=pkg-config \
--prefix=$PWD/../ffmpeg_sdk_android \
--enable-static \
--enable-shared \
--enable-small \
--disable-debug \
--disable-yasm \
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
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--extra-cflags="-D__ANDROID__ -fPIC" \
--extra-ldflags="-nostdlib" \

sed -i 's/HAVE_LRINT 0/HAVE_LRINT 1/g' config.h
sed -i 's/HAVE_LRINTF 0/HAVE_LRINTF 1/g' config.h
sed -i 's/HAVE_ROUND 0/HAVE_ROUND 1/g' config.h
sed -i 's/HAVE_ROUNDF 0/HAVE_ROUNDF 1/g' config.h
sed -i 's/HAVE_TRUNC 0/HAVE_TRUNC 1/g' config.h
sed -i 's/HAVE_TRUNCF 0/HAVE_TRUNCF 1/g' config.h
sed -i 's/HAVE_CBRT 0/HAVE_CBRT 1/g' config.h
sed -i 's/HAVE_CBRTF 0/HAVE_CBRTF 1/g' config.h
sed -i 's/HAVE_ISINF 0/HAVE_ISINF 1/g' config.h
sed -i 's/HAVE_ISNAN 0/HAVE_ISNAN 1/g' config.h
sed -i 's/HAVE_SINF 0/HAVE_SINF 1/g' config.h
sed -i 's/HAVE_RINT 0/HAVE_RINT 1/g' config.h
sed -i 's/#define av_restrict restrict/#define av_restrict/g' config.h

make -j8 && make install

cd -

