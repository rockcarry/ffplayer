// 包含头文件
#include "com_rockcarry_ffplayer_player.h"

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;Ljava/lang/Object;II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeOpen
  (JNIEnv *env, jclass clazz, jstring url, jobject surface, jint w, jint h)
{
    return 0;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeClose
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeClose
  (JNIEnv *env, jclass clazz, jint hplayer)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePlay
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePlay
  (JNIEnv *env, jclass clazz, jint hplayer)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePause
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePause
  (JNIEnv *env, jclass clazz, jint player)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSeek
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSeek
  (JNIEnv *env, jclass clazz, jint player, jint ms)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetParam
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetParam
  (JNIEnv *env, jclass clazz, jint player, jint id, jint value)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeGetParam
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeGetParam
  (JNIEnv *env, jclass clazz, jint player, jint id)
{
    return 0;
}
