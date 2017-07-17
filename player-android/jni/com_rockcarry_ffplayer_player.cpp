// 包含头文件
#include "com_rockcarry_ffplayer_player.h"
#include "ffplayer.h"

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeInitCallback
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeInitCallback
  (JNIEnv *env, jobject obj, jint hplayer)
{
    // todo..
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;Ljava/lang/Object;II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeOpen
  (JNIEnv *env, jclass clazz, jstring url, jobject jsurface, jint w, jint h)
{
    const char *str = env->GetStringUTFChars(url, NULL);
    sp<ANativeWindow> win;
    if (jsurface) {
        win = android_view_Surface_getNativeWindow(env, jsurface).get();
    }
    jint hplayer = (jint)player_open((char*)str, win);
    return hplayer;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeClose
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeClose
  (JNIEnv *env, jclass clazz, jint hplayer)
{
    player_close((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePlay
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePlay
  (JNIEnv *env, jclass clazz, jint hplayer)
{
    player_play((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePause
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePause
  (JNIEnv *env, jclass clazz, jint hplayer)
{
    player_pause((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSeek
 * Signature: (IJ)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSeek
  (JNIEnv *env, jclass clazz, jint hplayer, jlong ms)
{
    player_seek((void*)hplayer, ms);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetParam
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetParam
  (JNIEnv *env, jclass clazz, jint hplayer, jint id, jint value)
{
    player_setparam((void*)hplayer, id, &value);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeGetParam
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeGetParam
  (JNIEnv *env, jclass clazz, jint hplayer, jint id)
{
    int value = 0;
    player_getparam((void*)hplayer, id, &value);
    return value;
}
