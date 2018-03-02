// 包含头文件
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
typedef struct
{
    // common members
    VDEV_COMMON_MEMBERS

    // android natvie window
    ANativeWindow *win;

    AVFrame    *frames;
    SwsContext *swsctxt;
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

static void* video_render_thread_proc(void *param)
{
    VDEVCTXT *c = (VDEVCTXT*)param;

    while (1) {
        sem_wait(&c->semr);
        if (c->status & VDEV_CLOSE) break;

        int64_t vpts = c->vpts = c->ppts[c->head];
        if (vpts != -1 && c->win) {
            AVFrame *picture = &c->frames[c->head];
            ANativeWindow_Buffer winbuf;
            ANativeWindow_lock(c->win, &winbuf, NULL);
            uint8_t *data[8]     = { (uint8_t*)winbuf.bits };
            int      linesize[8] = { winbuf.stride * 4 };
            sws_scale(c->swsctxt, picture->data, picture->linesize, 0, c->sh, data, linesize);
            ANativeWindow_unlockAndPost(c->win);
        }

        av_log(NULL, AV_LOG_DEBUG, "vpts: %lld\n", vpts);
        if (++c->head == c->bufnum) c->head = 0;
        sem_post(&c->semw);

        // handle complete, av-sync & frame rate control
        vdev_handle_complete_and_avsync(c);
    }

    // need call DetachCurrentThread
    get_jni_jvm()->DetachCurrentThread();
    return NULL;
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
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->surface   = surface;
    ctxt->bufnum    = bufnum;
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

    // alloc buffer & semaphore
    ctxt->ppts   = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->frames = (AVFrame*)calloc(bufnum, sizeof(AVFrame));
    if (!ctxt->ppts || !ctxt->frames) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate memory for ppts & frames !\n");
        exit(0);
    }

    for (i=0; i<ctxt->bufnum; i++) {
        ctxt->frames[i].format = ctxt->pixfmt;
        ctxt->frames[i].width  = ctxt->sw;
        ctxt->frames[i].height = ctxt->sh;
        ret = av_frame_get_buffer(&ctxt->frames[i], 32);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "could not allocate frame data.\n");
            exit(0);
        }
    }
    ctxt->swsctxt = sws_getContext(
                ctxt->sw, ctxt->sh, (enum AVPixelFormat)ctxt->pixfmt,
                ctxt->sw, ctxt->sh, (enum AVPixelFormat)ctxt->pixfmt,
                SWS_FAST_BILINEAR, 0, 0, 0);

    // create semaphore
    sem_init(&ctxt->semr, 0, 0     );
    sem_init(&ctxt->semw, 0, bufnum);

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}

void vdev_android_destroy(void *ctxt)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    JNIEnv *env = get_jni_env();
    int       i;

    // make visual effect & rendering thread safely exit
    c->status = VDEV_CLOSE;
    sem_post(&c->semr);
    pthread_join(c->thread, NULL);

    // release android native window
    if (c->win) ANativeWindow_release(c->win);

    // close semaphore
    sem_destroy(&c->semr);
    sem_destroy(&c->semw);

    // unref frames
    for (i=0; i<c->bufnum; i++) {
        av_frame_unref(&c->frames[i]);
    }

    // free sws context
    if (c->swsctxt) {
        sws_freeContext(c->swsctxt);
    }
    //-- video --//

    // free memory
    free(c->ppts  );
    free(c->frames);
    free(c);
}

void vdev_android_dequeue(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    AVFrame  *p = NULL;
    sem_wait(&c->semw);
    p = &c->frames[c->tail];
    memcpy(buffer  , p->data    , 8 * sizeof(uint8_t*));
    memcpy(linesize, p->linesize, 8 * sizeof(int     ));
}

void vdev_android_enqueue(void *ctxt, int64_t pts)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;

    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    sem_post(&c->semr);
}

void vdev_android_setrect(void *ctxt, int x, int y, int w, int h)
{
    DO_USE_VAR(ctxt);
    DO_USE_VAR(x   );
    DO_USE_VAR(y   );
    DO_USE_VAR(w   );
    DO_USE_VAR(h   );
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

