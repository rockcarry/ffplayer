LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libffplayer_jni

LOCAL_SRC_FILES := \
    com_rockcarry_ffplayer_player.cpp \
    ../../src/ffplayer.cpp \
    ../../src/ffrender.cpp \
    ../../src/pktqueue.cpp \
    ../../src/vdev-cmn.cpp \
    ../../src/vdev-android.cpp \
    ../../src/adev-android.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../ffmpeg-android/include \
    $(LOCAL_PATH)/../../src

LOCAL_CFLAGS += \
    -O3 -mthumb -pipe -fpic -fasm \
    -finline-limit=300 -ffast-math \
    -fstrict-aliasing -Werror=strict-aliasing \
    -fmodulo-sched -fmodulo-sched-allow-regmoves \
    -Wno-psabi -Wa,--noexecstack \
    -D__ARM_ARCH_5__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5TE__ \
    -DANDROID -DNDEBUG -D__STDC_CONSTANT_MACROS \
    -mfpu=vfpv3-d16 -mfloat-abi=softfp

LOCAL_LDFLAGS += \
    -Wl,--fix-cortex-a8 \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavformat.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavcodec.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavdevice.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libswresample.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libswscale.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavutil.a

LOCAL_SHARED_LIBRARIES := libcutils libutils libui libgui libandroid_runtime libz

include $(BUILD_SHARED_LIBRARY)

