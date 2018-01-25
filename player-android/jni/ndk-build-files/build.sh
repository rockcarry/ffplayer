#!/bin/bash
set -e

BASEDIR=$PWD

echo ""
echo "building ffplayer jni library using ndk..."
echo ""

mv $BASEDIR/../Android.mk $BASEDIR/../Android.dat
cp $BASEDIR/Android.dat $BASEDIR/../Android.mk
cp $BASEDIR/Application.dat $BASEDIR/../Application.mk

cd $BASEDIR/..

export PATH=$PATH:$NDK_HOME
ndk-build

cd $BASEDIR

rm $BASEDIR/../Android.mk
rm $BASEDIR/../Application.mk
mv $BASEDIR/../Android.dat $BASEDIR/../Android.mk

echo ""
echo "build ffplayer jni library done !"
echo ""
