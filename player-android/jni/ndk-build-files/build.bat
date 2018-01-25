@echo off
@echo building ffplayer jni library using ndk...
@echo.

set pwd=%~dp0

rename %pwd%..\Android.mk Android.dat
copy Android.dat %pwd%..\Android.mk
copy Application.dat %pwd%..\Application.mk

cd %pwd%..

set PATH=%PATH%;%NDK_HOME%
call ndk-build

cd %pwd%

del %pwd%..\Android.mk
del %pwd%..\Application.mk
rename %pwd%..\Android.dat Android.mk

@echo.
@echo build ffplayer jni library done !
@echo.

pause


