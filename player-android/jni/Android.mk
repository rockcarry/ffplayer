LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libffplayer_jni

LOCAL_SRC_FILES := com_rockcarry_ffplayer_player.c

LOCAL_C_INCLUDES += $(JNI_H_INCLUDE)

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
