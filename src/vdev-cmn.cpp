// 包含头文件
#include "vdev.h"

// 函数实现
void vdev_pause(void *ctxt, int pause)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (pause) {
        c->status |=  VDEV_PAUSE;
    }
    else {
        c->status &= ~VDEV_PAUSE;
    }
}

void vdev_reset(void *ctxt)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    while (0 == sem_trywait(&c->semr)) {
        sem_post(&c->semw);
    }
    c->head   = c->tail =  0;
    c->apts   = c->vpts = -1;
    c->status = 0;
}

void vdev_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (ppapts) *ppapts = &c->apts;
    if (ppvpts) *ppvpts = &c->vpts;
}

void vdev_setparam(void *ctxt, int id, void *param)
{
    if (!ctxt || !param) return;
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;

    switch (id) {
    case PARAM_VDEV_FRAME_RATE:
        c->tickframe = 1000 / (*(int*)param > 1 ? *(int*)param : 1);
        break;
    case PARAM_PLAYER_CALLBACK:
        c->fpcb = (PFN_PLAYER_CALLBACK)param;
        break;
    case PARAM_AVSYNC_TIME_DIFF:
        c->tickavdiff = *(int*)param;
        break;
    }
}

void vdev_getparam(void *ctxt, int id, void *param)
{
    if (!ctxt || !param) return;
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;

    switch (id) {
    case PARAM_VDEV_FRAME_RATE:
        *(int*)param = 1000 / c->tickframe;
        break;
    case PARAM_AVSYNC_TIME_DIFF:
        *(int*)param = c->tickavdiff;
        break;
    }
}

void vdev_player_event(void *ctxt, int32_t msg, int64_t param)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    if (c->fpcb) {
        c->fpcb(msg, param);
    }
#ifdef WIN32
    else {
        if (msg == PLAY_COMPLETED) {
            PostMessage((HWND)c->pwnd, MSG_FFPLAYER, PLAY_COMPLETED, 0);
        }
    }
#endif
}

void vdev_refresh_background(void *ctxt)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    RECT rtwin, rect1, rect2, rect3, rect4;
    int  x = c->x, y = c->y, w = c->w, h = c->h;

#ifdef WIN32
    HWND hwnd = (HWND)c->pwnd;
    GetClientRect(hwnd, &rtwin);
    rect1.left = 0;   rect1.top = 0;   rect1.right = rtwin.right; rect1.bottom = y;
    rect2.left = 0;   rect2.top = y;   rect2.right = x;           rect2.bottom = y+h;
    rect3.left = x+w; rect3.top = y;   rect3.right = rtwin.right; rect3.bottom = y+h;
    rect4.left = 0;   rect4.top = y+h; rect4.right = rtwin.right; rect4.bottom = rtwin.bottom;
    InvalidateRect(hwnd, &rect1, TRUE);
    InvalidateRect(hwnd, &rect2, TRUE);
    InvalidateRect(hwnd, &rect3, TRUE);
    InvalidateRect(hwnd, &rect4, TRUE);
#endif

#ifdef ANDROID
    // todo...
#endif
}

void* vdev_create(int type, void *surface, int bufnum, int w, int h, int frate)
{
    VDEV_COMMON_CTXT *c = NULL;
#ifdef WIN32
    switch (type) {
    case VDEV_RENDER_TYPE_GDI: c = (VDEV_COMMON_CTXT*)vdev_gdi_create(surface, bufnum, w, h, frate); break;
    case VDEV_RENDER_TYPE_D3D: c = (VDEV_COMMON_CTXT*)vdev_d3d_create(surface, bufnum, w, h, frate); break;
    }
#endif
#ifdef ANDROID
    c = (VDEV_COMMON_CTXT*)vdev_android_create(surface, bufnum, w, h, frate);
#endif
    if (c) c->type = type;
    return c;
}

void vdev_destroy(void *ctxt)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
#ifdef WIN32
    switch (c->type) {
    case VDEV_RENDER_TYPE_GDI: vdev_gdi_destroy(ctxt); break;
    case VDEV_RENDER_TYPE_D3D: vdev_d3d_destroy(ctxt); break;
    }
#endif
#ifdef ANDROID
    vdev_android_destroy(ctxt);
#endif
}

void vdev_request(void *ctxt, void **buf, int *stride)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
#ifdef WIN32
    switch (c->type) {
    case VDEV_RENDER_TYPE_GDI: vdev_gdi_request(ctxt, buf, stride); break;
    case VDEV_RENDER_TYPE_D3D: vdev_d3d_request(ctxt, buf, stride); break;
    }
#endif
#ifdef ANDROID
    vdev_android_request(ctxt, buf, stride);
#endif
}

void vdev_post(void *ctxt, int64_t pts)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
#ifdef WIN32
    switch (c->type) {
    case VDEV_RENDER_TYPE_GDI: vdev_gdi_post(ctxt, pts); break;
    case VDEV_RENDER_TYPE_D3D: vdev_d3d_post(ctxt, pts); break;
    }
#endif
#ifdef ANDROID
    vdev_android_post(ctxt, pts);
#endif
}

void vdev_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
#ifdef WIN32
    switch (c->type) {
    case VDEV_RENDER_TYPE_GDI: vdev_gdi_setrect(ctxt, x, y, w, h); break;
    case VDEV_RENDER_TYPE_D3D: vdev_d3d_setrect(ctxt, x, y, w, h); break;
    }
#endif
#ifdef ANDROID
    vdev_android_setrect(ctxt, x, y, w, h);
#endif
}

