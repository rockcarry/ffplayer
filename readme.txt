ffplayer
========

A video player based on ffmpeg for windows and android platform.

ffplayer is not ffplay in offical ffmpeg source code. It is a simple player implemention on windows vs2005 platform. It is a light weight implemention without SDL, with directly access to bitmaps and wavdev by Win32 API.

Currently, this player can smoothly play many video files, with low cpu and memory usage (compared with ffplay of offical ffmpeg), and the audio/video also synchronized very well. More effort must be done to optimize things on hwaccel, performance, compatibility and stability.

The ffmpeg share library (dlls, libs & headers) are customized and built by myself, and I will regularly update it if ffmpeg has a major release. I write a simple guide for how to build ffmpeg on mingw64 platform, you can find it on ffplayer's wiki.

The windows version become stable, and the android version is coming. The android verion ffplayer is working now, but lots of work need to be done, to make it better.


features
========
1. low memory and cpu usage
2. audio/video synchronized very well
3. gdi and direct3d rendering on win32 platform
4. support variable speed playback 1% to 100%, 200% ..
5. waveform and spectrum visual effect
6. support stream selection
7. support network media stream playback
8. support dshow, gdigrab and vfwcap (avdevice of ffmpeg)


testplayer
==========
testplayer is a simple test player for ffplayer
hot-keys for testplayer:
ctrl+E - switch visual effect
ctrl+M - switch between letter box and stretch rect
ctrl+R - switch between gdi and d3d
ctrl+A - switch audio stream


contact and discuss
===================
email   : rockcarry@163.com
qq group: 383930765


rockcarry
2016.1.7

