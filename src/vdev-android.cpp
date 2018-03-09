// 包含头文件
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

// for jni
JNIEXPORT JavaVM* get_jni_jvm(void);
JNIEXPORT JNIEnv* get_jni_env(void);

// 内部常量定义
#define DEF_VDEV_BUF_NUM  3
#define DEF_WIN_PIX_FMT   WINDOW_FORMAT_RGBX_8888

// 内部类型定义
typedef struct {
    // common members
    VDEV_COMMON_MEMBERS

    ANativeWindow *win;
    SwsContext    *sws;
} VDEVCTXT;

// 内部函数实现
inline int android_pixfmt_to_ffmpeg_pixfmt(int srcfmt)
{
    // dst fmt
    int dst_fmt = 0;
    switch (srcfmt) {
    case WINDOW_FORMAT_RGB_565:   dst_fmt = AV_PIX_FMT_RGB565; break;
    case WINDOW_FORMAT_RGBX_8888: dst_fmt = AV_PIX_FMT_BGR32;  break;
    }
    return dst_fmt;
}

void vdev_android_setparam(void *ctxt, int id, void *param)
{
    if (!ctxt || !param) return;
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    switch (id) {
    case PARAM_VDEV_POST_SURFACE:
        {
            AVFrame *picture = (AVFrame*)param;
            if (c->sws == NULL) {
                c->sws = sws_getContext(
                    c->sw, c->sh, (AVPixelFormat)picture->format,
                    c->sw, c->sh, (AVPixelFormat)c->pixfmt,
                    SWS_FAST_BILINEAR, 0, 0, 0);
            }

            if (c->win) {
                ANativeWindow_Buffer winbuf;
                ANativeWindow_lock(c->win, &winbuf, NULL);
                uint8_t *data[8]     = { (uint8_t*)winbuf.bits };
                int      linesize[8] = { winbuf.stride * 4 };
                sws_scale(c->sws, picture->data, picture->linesize, 0, c->sh, data, linesize);
                ANativeWindow_unlockAndPost(c->win);
            }

            c->vpts = picture->pts;
            vdev_avsync_and_complete(c);
        }
        break;
    }
}

static void vdev_android_destroy(void *ctxt)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;

    // release android native window
    if (c->win) ANativeWindow_release(c->win);

    // free sws context
    if (c->sws) {
        sws_freeContext(c->sws);
    }

    free(c);
}

// 接口函数实现
void* vdev_android_create(void *surface, int bufnum, int w, int h, int frate)
{
    int ret, i;
    VDEVCTXT *ctxt = (VDEVCTXT*)calloc(1, sizeof(VDEVCTXT));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate vdev context !\n");
        exit(0);
    }

    // init vdev context
    ctxt->pixfmt    = android_pixfmt_to_ffmpeg_pixfmt(DEF_WIN_PIX_FMT);
    ctxt->w         = w ? w : 1;
    ctxt->h         = h ? h : 1;
    ctxt->sw        = w ? w : 1;
    ctxt->sh        = h ? h : 1;
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;
    ctxt->apts      = -1;
    ctxt->vpts      = -1;
    ctxt->tickavdiff= -ctxt->tickframe * 4; // 4 should equals to (DEF_ADEV_BUF_NUM - 1)
    ctxt->setparam  = vdev_android_setparam;
    ctxt->destroy   = vdev_android_destroy;

    return ctxt;
}

void vdev_android_setwindow(void *ctxt, jobject surface)
{
    if (!ctxt) return;
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    JNIEnv *env = get_jni_env();

    ANativeWindow *win = c->win;
    c->win = NULL;

    // release old native window
    if (win) ANativeWindow_release(win);

    // create new native window
    win = surface ? ANativeWindow_fromSurface(env, surface) : NULL;
    if (win) {
        ANativeWindow_acquire(win);
        ANativeWindow_setBuffersGeometry(win, c->sw, c->sh, DEF_WIN_PIX_FMT);
    }
    c->win = win;
}
