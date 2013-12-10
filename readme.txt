ffplayer
========

A windows video player based on ffmpeg.

ffplayer is not ffplay in offical ffmpeg source code. It is a simple player implement on windows vs2005 platform. It is a light weight implement without SDL, with directly access to bitmaps and wavdev in Win32 API.

Currently, this player can stupidly play some video files, but sometime with lags or audio and video are not synchronized. More effort must be done to optimize things on synchronization, performance, compatibility and stability.

The ffmpeg share library (dlls, libs & headers) is come from ffmpeg offical web site. I will regularly update these files. Other files are all written by my self. These source codes are licensed under GPL.

After windows version is stable, I will port it to android platform.


rockcarry
2013.12.10


