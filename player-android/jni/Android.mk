LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libffplayer_jni

LOCAL_SRC_FILES := com_rockcarry_ffplayer_player.cpp

LOCAL_C_INCLUDES += $(JNI_H_INCLUDE)

LOCAL_SHARED_LIBRARIES := libcutils libutils

include $(BUILD_SHARED_LIBRARY)
