#include <jni.h>
#include "com_rockcarry_ffplayer_player.h"

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    open
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_rockcarry_ffplayer_player_open
  (JNIEnv *env, jobject this, jstring url, jobject surface)
{
    return 1;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_close
  (JNIEnv *env, jobject this)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    play
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_play
  (JNIEnv *env, jobject this)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    pause
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_pause
  (JNIEnv *env, jobject this)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    seek
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_seek
  (JNIEnv *env, jobject this, jlong sec)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    setParam
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_setParam
  (JNIEnv *env, jobject this, jint id, jint value)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    getParam
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_getParam
  (JNIEnv *env, jobject this, jint id)
{
    return 0;
}
