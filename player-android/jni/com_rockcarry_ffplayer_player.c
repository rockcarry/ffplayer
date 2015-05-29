#include <jni.h>
#include <android_runtime/AndroidRuntime.h>
#include <gui/Surface.h>
#include "com_rockcarry_ffplayer_player.h"

typedef struct {
    Surface *surface;
} PLAYER;

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    open
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_com_rockcarry_ffplayer_player_open
  (JNIEnv *env, jobject this, jstring url, jobject surface)
{
    return 0;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    close
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_close
  (JNIEnv *env, jobject this, jobject player)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    play
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_play
  (JNIEnv *env, jobject this, jobject player)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    pause
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_pause
  (JNIEnv *env, jobject this, jobject player)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    seek
 * Signature: (Ljava/lang/Object;J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_seek
  (JNIEnv *env, jobject this, jobject player, jlong sec)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    setparam
 * Signature: (Ljava/lang/Object;II)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_setparam
  (JNIEnv *env, jobject this, jobject player, jint id, jint value)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    getparam
 * Signature: (Ljava/lang/Object;I)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_getparam
  (JNIEnv *env, jobject this, jobject player, jint id)
{
}
