#!/bin/bash
set -e

BASEDIR=$PWD

echo ""
echo "building ffplayer jni library using ndk..."
echo ""

export PATH=$PATH:$NDK_HOME
ndk-build

cd $BASEDIR/../libs/
mkdir -p $BASEDIR/../apk/app/src/main/jniLibs/
find -name libffplayer_jni.so | xargs -i cp -arf --parent {} $BASEDIR/../apk/app/src/main/jniLibs/
cd -

rm -rf $BASEDIR/../libs/ $BASEDIR/../obj/

echo ""
echo "build ffplayer jni library done !"
echo ""

