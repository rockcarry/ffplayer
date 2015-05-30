#include <jni.h>
#include <gui/Surface.h>
#include "com_rockcarry_ffplayer_player.h"
using namespace android;

typedef struct {
    Surface *surface;
} PLAYER;

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    open
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_rockcarry_ffplayer_player_open
  (JNIEnv *env, jobject obj, jstring url, jobject surface)
{
    jclass   cls = env->GetObjectClass(obj);
    jfieldID id  = env->GetFieldID(cls , "context" , "Ljava/lang/Object;");
    if (id == NULL) ALOGW("can't find id of 'context'.");

    return true;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_close
  (JNIEnv *env, jobject obj)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    play
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_play
  (JNIEnv *env, jobject obj)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    pause
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_pause
  (JNIEnv *env, jobject obj)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    seek
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_seek
  (JNIEnv *env, jobject obj, jlong sec)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    setParam
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_setParam
  (JNIEnv *env, jobject obj, jint id, jint value)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    getParam
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_getParam
  (JNIEnv *env, jobject obj, jint id)
{
    return 0;
}
