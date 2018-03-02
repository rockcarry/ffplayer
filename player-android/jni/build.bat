@echo off
@echo building ffplayer jni library using ndk...
@echo.

set pwd=%~dp0

set PATH=%PATH%;%NDK_HOME%
call ndk-build

xcopy %pwd%..\libs %pwd%..\apk\app\src\main\jniLibs\ /s /y
rd %pwd%..\libs %pwd%..\obj /s /q

@echo.
@echo build ffplayer jni library done !
@echo.

pause

