LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libffplayer_jni

LOCAL_SRC_FILES := \
    com_rockcarry_ffplayer_player.cpp \
    ../../src/ffplayer.cpp \
    ../../src/ffrender.cpp \
    ../../src/pktqueue.cpp \
    ../../src/recorder.cpp \
    ../../src/adev-cmn.cpp \
    ../../src/adev-android.cpp \
    ../../src/vdev-cmn.cpp \
    ../../src/vdev-android.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../ffmpeg-android/include \
    $(LOCAL_PATH)/../../src

LOCAL_CFLAGS += -DANDROID -DNDEBUG -D__STDC_CONSTANT_MACROS -Os -mfpu=neon-vfpv4 -mfloat-abi=softfp
LOCAL_LDLIBS += -lz

LOCAL_LDFLAGS += -ldl \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavformat.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavcodec.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavdevice.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavfilter.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libswresample.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libswscale.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libavutil.a \
    $(LOCAL_PATH)/../ffmpeg-android/lib/libx264.a

LOCAL_SHARED_LIBRARIES += libcutils libutils libui libgui libandroid_runtime

LOCAL_MULTILIB := 32

include $(BUILD_SHARED_LIBRARY)

