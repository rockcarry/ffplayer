#define LOG_TAG "ffplayer_jni"

#include <jni.h>
#include <android_runtime/AndroidRuntime.h>
#include <android_runtime/android_view_Surface.h>
#include <gui/Surface.h>
#include "com_rockcarry_ffplayer_player.h"
using namespace android;

typedef struct {
    sp<Surface> surface;
} PLAYER;

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeOpen
  (JNIEnv *env, jclass cls, jstring url, jobject surface)
{
    ALOGD("Java_com_rockcarry_ffplayer_player_open.");
    PLAYER *player = new PLAYER();
    player->surface = android_view_Surface_getSurface(env, surface);
    if (android::Surface::isValid(player->surface)) {
		ALOGE("surface is valid .");
	} else {
		ALOGE("surface is invalid.");
	}
    return (jint)player;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeClose
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeClose
  (JNIEnv *env, jclass cls, jint hplayer)
{
    PLAYER *player = (PLAYER*)hplayer;
    delete player;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePlay
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePlay
  (JNIEnv *env, jclass cls, jint hplayer)
{
    PLAYER *player = (PLAYER*)hplayer;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePause
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePause
  (JNIEnv *env, jclass cls, jint hplayer)
{
    PLAYER *player = (PLAYER*)hplayer;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSeek
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSeek
  (JNIEnv *env, jclass cls, jint hplayer, jint sec )
{
    PLAYER *player = (PLAYER*)hplayer;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetParam
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetParam
  (JNIEnv *env, jclass cls, jint hplayer, jint id, jint value)
{
    PLAYER *player = (PLAYER*)hplayer;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeGetParam
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeGetParam
  (JNIEnv *env, jclass cls, jint hplayer, jint id)
{
    return 0;
}

