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

static int parse_params(const char *str, const char *key)
{
    char  t[12];
    char *p = strstr(str, key);
    int   i;

    if (!p) return 0;
    p += strlen(key);
    if (*p == '\0') return 0;

    while (1) {
        if (*p != ' ' && *p != '=' && *p != ':') break;
        else p++;
    }

    for (i=0; i<12; i++) {
        if (*p == ',' || *p == ';' || *p == '\n' || *p == '\0') {
            t[i] = '\0';
            break;
        } else {
            t[i] = *p++;
        }
    }
    t[11] = '\0';
    return atoi(t);
}

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
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;Ljava/lang/Object;IILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativeOpen
  (JNIEnv *env, jobject obj, jstring url, jobject jsurface, jint w, jint h, jstring params)
{
    DO_USE_VAR(obj);
    DO_USE_VAR(jsurface);
    DO_USE_VAR(w);
    DO_USE_VAR(h);

    PLAYER_INIT_PARAMS playerparams;
    memset(&playerparams, 0, sizeof(playerparams));
    if (params != NULL) {
        const char *strparams = env->GetStringUTFChars(params, NULL);
        player_load_init_params(&playerparams, strparams);
        env->ReleaseStringUTFChars(params, strparams);
    }

    const char *strurl = env->GetStringUTFChars(url, NULL);
    jlong hplayer = (jlong)player_open((char*)strurl, obj, &playerparams);
    env->ReleaseStringUTFChars(url, strurl);
    return hplayer;
}

/*
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativeClose
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativeClose
  (JNIEnv *env, jobject obj, jlong hplayer)
{
    DO_USE_VAR(env);
    DO_USE_VAR(obj);
    player_close((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativePlay
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativePlay
  (JNIEnv *env, jobject obj, jlong hplayer)
{
    DO_USE_VAR(env);
    DO_USE_VAR(obj);
    player_play((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativePause
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativePause
  (JNIEnv *env, jobject obj, jlong hplayer)
{
    DO_USE_VAR(env);
    DO_USE_VAR(obj);
    player_pause((void*)hplayer);
}

/*
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativeSeek
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativeSeek
  (JNIEnv *env, jobject obj, jlong hplayer, jlong ms)
{
    DO_USE_VAR(env);
    DO_USE_VAR(obj);
    player_seek((void*)hplayer, ms, SEEK_PRECISELY);
}

/*
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativeSetParam
 * Signature: (JIJ)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativeSetParam
  (JNIEnv *env, jobject obj, jlong hplayer, jint id, jlong value)
{
    DO_USE_VAR(env);
    DO_USE_VAR(obj);
    player_setparam((void*)hplayer, id, &value);
}

/*
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativeGetParam
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativeGetParam
  (JNIEnv *env, jobject obj, jlong hplayer, jint id)
{
    DO_USE_VAR(env);
    DO_USE_VAR(obj);
    jlong value = 0;
    player_getparam((void*)hplayer, id, &value);
    return value;
}

/*
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativeSetDisplaySurface
 * Signature: (JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativeSetDisplaySurface
  (JNIEnv *env, jobject obj, jlong hplayer, jobject jsurface)
{
    DO_USE_VAR(obj);
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
 * Class:     com_rockcarry_ffplayer_MediaPlayer
 * Method:    nativeSetDisplayTexture
 * Signature: (JLjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_MediaPlayer_nativeSetDisplayTexture
  (JNIEnv *env, jobject obj, jlong hplayer, jobject jtexture)
{
    DO_USE_VAR(obj);
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


