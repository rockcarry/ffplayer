// 包含头文件
#include <gui/Surface.h>
#include <android_runtime/android_view_Surface.h>
#include <android_runtime/android_graphics_SurfaceTexture.h>
#include "com_rockcarry_ffplayer_player.h"
#include "ffplayer.h"
#include "adev.h"
#include "vdev.h"

using namespace android;

// this function defined in libavcodec/jni.h
extern "C" int av_jni_set_java_vm(void *vm, void *log_ctx);


JavaVM* g_jvm = NULL;
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    DO_USE_VAR(reserved);

    JNIEnv* env = NULL;
    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        return -1;
    }
    assert(env != NULL);

    // for g_jvm
    g_jvm = vm;
    av_jni_set_java_vm(vm, NULL);
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
    if (status != JNI_OK) {
//      ALOGD("failed to get JNI environment assuming native thread !\n");
        status = g_jvm->AttachCurrentThread(&env, NULL);
        if (status != JNI_OK) {
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
    adev_android_initjni(adev, env, obj);
    vdev_android_initjni(vdev, env, obj);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;Ljava/lang/Object;II)J
 */
JNIEXPORT jlong JNICALL Java_com_rockcarry_ffplayer_player_nativeOpen
  (JNIEnv *env, jclass clazz, jstring url, jobject jsurface, jint w, jint h)
{
    DO_USE_VAR(clazz);
    DO_USE_VAR(jsurface);
    DO_USE_VAR(w);
    DO_USE_VAR(h);
    const char *str = env->GetStringUTFChars(url, NULL);
    jlong hplayer = (jlong)player_open((char*)str, NULL);
    env->ReleaseStringUTFChars(url, str);
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
    DO_USE_VAR(env);
    DO_USE_VAR(clazz);
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
    DO_USE_VAR(env);
    DO_USE_VAR(clazz);
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
    DO_USE_VAR(env);
    DO_USE_VAR(clazz);
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
    DO_USE_VAR(env);
    DO_USE_VAR(clazz);
    player_seek((void*)hplayer, ms, SEEK_PRECISELY);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetParam
 * Signature: (JIJ)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetParam
  (JNIEnv *env, jclass clazz, jlong hplayer, jint id, jlong value)
{
    DO_USE_VAR(env);
    DO_USE_VAR(clazz);
    player_setparam((void*)hplayer, id, &value);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeGetParam
 * Signature: (II)J
 */
JNIEXPORT jlong JNICALL Java_com_rockcarry_ffplayer_player_nativeGetParam
  (JNIEnv *env, jclass clazz, jlong hplayer, jint id)
{
    DO_USE_VAR(env);
    DO_USE_VAR(clazz);
    jlong value = 0;
    player_getparam((void*)hplayer, id, &value);
    return value;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetDisplaySurface
 * Signature: (JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetDisplaySurface
  (JNIEnv *env, jclass clazz, jlong hplayer, jobject jsurface)
{
    DO_USE_VAR(clazz);
    void *vdev = NULL;
    player_getparam((void*)hplayer, PARAM_VDEV_GET_CONTEXT, &vdev);
    sp<IGraphicBufferProducer> gbp;
    sp<Surface> surface;
    if (jsurface) {
        surface = android_view_Surface_getSurface(env, jsurface);
        if (surface != NULL) {
            gbp = surface->getIGraphicBufferProducer();
        }
    }
    vdev_android_setwindow(vdev, gbp);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetDisplayTexture
 * Signature: (JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetDisplayTexture
  (JNIEnv *env, jclass clazz, jlong hplayer, jobject jtexture)
{
    DO_USE_VAR(clazz);
    void *vdev = NULL;
    player_getparam((void*)hplayer, PARAM_VDEV_GET_CONTEXT, &vdev);
    sp<IGraphicBufferProducer> gbp = NULL;
    if (jtexture != NULL) {
        gbp = SurfaceTexture_getProducer(env, jtexture);
        if (gbp == NULL) {
            ALOGW("SurfaceTexture already released in setPreviewTexture !");
            return;
        }
    }
    vdev_android_setwindow(vdev, gbp);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeEnableCallback
 * Signature: (JI)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeEnableCallback
  (JNIEnv *env, jclass clazz, jlong hplayer, jint enable)
{
    DO_USE_VAR(env);
    DO_USE_VAR(clazz);
    player_setparam((void*)hplayer, PARAM_PLAYER_CALLBACK, (void*)(enable ? vdev_android_default_player_callback : NULL));
}
