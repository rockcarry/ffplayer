#include <jni.h>
#include "com_rockcarry_ffplayer_player.h"

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    open
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_com_rockcarry_ffplayer_player_open
  (JNIEnv *, jobject, jstring, jobject)
{
    return NULL;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    close
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_close
  (JNIEnv *, jobject, jobject)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    play
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_play
  (JNIEnv *, jobject, jobject)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    pause
 * Signature: (Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_pause
  (JNIEnv *, jobject, jobject)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    seek
 * Signature: (Ljava/lang/Object;J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_seek
  (JNIEnv *, jobject, jobject, jlong)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    setparam
 * Signature: (Ljava/lang/Object;II)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_setparam
  (JNIEnv *, jobject, jobject, jint, jint)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    getparam
 * Signature: (Ljava/lang/Object;I)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_getparam
  (JNIEnv *, jobject, jobject, jint)
{
}
