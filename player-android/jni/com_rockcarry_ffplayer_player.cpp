// 包含头文件
#include "com_rockcarry_ffplayer_player.h"
#include "ffplayer.h"
#include "adev.h"
#include "vdev.h"

JavaVM* g_jvm = NULL;
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        return -1;
    }
    assert(env != NULL);

    // for g_jvm
    g_jvm = vm;

    return JNI_VERSION_1_4;
}

JNIEXPORT JNIEnv* get_jni_env(void)
{
    JNIEnv *env;
    int status;
    if (NULL == g_jvm)
    {
        ALOGE("g_jvm == NULL !\n");
        return NULL;
    }
    status = g_jvm->GetEnv((void **)&env, JNI_VERSION_1_4);
    if (status < 0) {
//      ALOGD("failed to get JNI environment assuming native thread !\n");
        status = g_jvm->AttachCurrentThread(&env, NULL);
        if (status < 0) {
            ALOGE("failed to attach current thread !\n");
            return NULL;
        }
    }
    return env;
}



/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeInitJniObject
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeInitJniObject
  (JNIEnv *env, jobject obj, jlong hplayer)
{
    void *adev = NULL;
    void *vdev = NULL;
    player_getparam((void*)hplayer, PARAM_ADEV_GET_CONTEXT, &adev);
    player_getparam((void*)hplayer, PARAM_VDEV_GET_CONTEXT, &vdev);
    adev_setjniobj(adev, env, obj);
    vdev_setjniobj(vdev, env, obj);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;Ljava/lang/Object;II)J
 */
JNIEXPORT jlong JNICALL Java_com_rockcarry_ffplayer_player_nativeOpen
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
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeClose
  (JNIEnv *env, jclass clazz, jlong hplayer)
{
    player_close((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePlay
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePlay
  (JNIEnv *env, jclass clazz, jlong hplayer)
{
    player_play((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePause
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePause
  (JNIEnv *env, jclass clazz, jlong hplayer)
{
    player_pause((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSeek
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSeek
  (JNIEnv *env, jclass clazz, jlong hplayer, jlong ms)
{
    player_seek((void*)hplayer, ms);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetParam
 * Signature: (JII)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetParam
  (JNIEnv *env, jclass clazz, jlong hplayer, jint id, jint value)
{
    player_setparam((void*)hplayer, id, &value);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeGetParam
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeGetParam
  (JNIEnv *env, jclass clazz, jlong hplayer, jint id)
{
    int value = 0;
    player_getparam((void*)hplayer, id, &value);
    return value;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetDisplayWindow
 * Signature: (JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetDisplayWindow
  (JNIEnv *env, jclass clazz, jlong hplayer, jobject win)
{
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetDisplayTarget
 * Signature: (JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetDisplayTarget
  (JNIEnv *env, jclass clazz, jlong hplayer, jobject win)
{
}




