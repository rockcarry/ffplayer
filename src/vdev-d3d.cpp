// 包含头文件
#include <d3d9.h>
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 预编译开关
#define ENABLE_WAIT_D3D_VSYNC  FALSE

// 内部常量定义
#define DEF_VDEV_BUF_NUM  3

// 内部类型定义
typedef struct
{
    // common members
    VDEV_COMMON_MEMBERS

    LPDIRECT3D9           pD3D9;
    LPDIRECT3DDEVICE9     pD3DDev;
    LPDIRECT3DSURFACE9   *pSurfs;
    D3DPRESENT_PARAMETERS d3dpp;
    D3DFORMAT             d3dfmt;
} VDEVD3DCTXT;

// 内部函数实现
static void d3d_draw_surf(VDEVD3DCTXT *c, LPDIRECT3DSURFACE9 surf)
{
    IDirect3DSurface9 *pBackBuffer = NULL;
    if (SUCCEEDED(c->pD3DDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
        if (pBackBuffer) {
            if (SUCCEEDED(c->pD3DDev->StretchRect(surf, NULL, pBackBuffer, NULL, D3DTEXF_LINEAR)))
            {
                RECT rect = { c->x, c->y, c->x + c->w, c->y + c->h };
                c->pD3DDev->Present(NULL, &rect, NULL, NULL);
            }
            pBackBuffer->Release();
        }
    }
}

static void* video_render_thread_proc(void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)param;

    while (1) {
        sem_wait(&c->semr);
        if (c->status & VDEV_CLOSE) break;

        if (c->refresh_flag) {
            c->refresh_flag = 0;
            vdev_refresh_background(c);
        }

        int64_t apts = c->apts;
        int64_t vpts = c->vpts = c->ppts[c->head];
#if CLEAR_VDEV_WHEN_COMPLETED
        if (vpts != -1 && !(c->status & VDEV_COMPLETED)) {
#else
        if (vpts != -1) {
#endif
            d3d_draw_surf(c, c->pSurfs[c->head]);
        }

        av_log(NULL, AV_LOG_DEBUG, "vpts: %lld\n", vpts);
        if (++c->head == c->bufnum) c->head = 0;
        sem_post(&c->semw);

        if (!(c->status & (VDEV_PAUSE|VDEV_COMPLETED))) {
            // send play progress event
            vdev_player_event(c, PLAY_PROGRESS, c->vpts > c->apts ? c->vpts : c->apts);

            //++ play completed ++//
            if (c->completed_apts != c->apts || c->completed_vpts != c->vpts) {
                c->completed_apts = c->apts;
                c->completed_vpts = c->vpts;
                c->completed_counter = 0;
            }
            else if (++c->completed_counter == 50) {
                av_log(NULL, AV_LOG_INFO, "play completed !\n");
                c->status |= VDEV_COMPLETED;
                vdev_player_event(c, PLAY_COMPLETED, 0);

#if CLEAR_VDEV_WHEN_COMPLETED
                InvalidateRect((HWND)c->hwnd, NULL, TRUE);
#endif
            }
            //-- play completed --//

            //++ frame rate & av sync control ++//
            DWORD   tickcur  = GetTickCount();
            int     tickdiff = tickcur - c->ticklast;
            int64_t avdiff   = apts - vpts - c->tickavdiff;
            c->ticklast = tickcur;
            if (tickdiff - c->tickframe >  2) c->ticksleep--;
            if (tickdiff - c->tickframe < -2) c->ticksleep++;
            if (apts != -1 && vpts != -1) {
                if (avdiff > 5) c->ticksleep-=2;
                if (avdiff <-5) c->ticksleep+=2;
            }
            if (c->ticksleep < 0) c->ticksleep = 0;
            if (c->ticksleep > 0) Sleep(c->ticksleep);
            av_log(NULL, AV_LOG_INFO, "d3d d: %3lld, s: %d\n", avdiff, c->ticksleep);
            //-- frame rate & av sync control --//
        }
        else Sleep(c->tickframe);
    }

    return NULL;
}

// 接口函数实现
void* vdev_d3d_create(void *surface, int bufnum, int w, int h, int frate)
{
    VDEVD3DCTXT *ctxt = (VDEVD3DCTXT*)calloc(1, sizeof(VDEVD3DCTXT));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate d3d vdev context !\n");
        exit(0);
    }

    // init vdev context
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->hwnd      = surface;
    ctxt->bufnum    = bufnum;
    ctxt->w         = w > 1 ? w : 1;
    ctxt->h         = h > 1 ? h : 1;
    ctxt->sw        = ctxt->w < GetSystemMetrics(SM_CXSCREEN) ? ctxt->w : GetSystemMetrics(SM_CXSCREEN);
    ctxt->sh        = ctxt->h < GetSystemMetrics(SM_CYSCREEN) ? ctxt->h : GetSystemMetrics(SM_CYSCREEN);
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;
    ctxt->apts      =-1;
    ctxt->vpts      =-1;

    // alloc buffer & semaphore
    ctxt->ppts   = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->pSurfs = (LPDIRECT3DSURFACE9*)calloc(bufnum, sizeof(LPDIRECT3DSURFACE9));

    // create semaphore
    sem_init(&ctxt->semr, 0, 0     );
    sem_init(&ctxt->semw, 0, bufnum);

    // create d3d
    ctxt->pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!ctxt->ppts || !ctxt->pSurfs || !ctxt->semr || !ctxt->semw || !ctxt->pD3D9) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for vdev-d3d !\n");
        exit(0);
    }

    // fill d3dpp struct
    D3DDISPLAYMODE d3dmode = {0};
    ctxt->pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3dmode);
    ctxt->d3dpp.BackBufferFormat      = D3DFMT_UNKNOWN;
    ctxt->d3dpp.BackBufferCount       = 1;
    ctxt->d3dpp.BackBufferWidth       = ctxt->sw;
    ctxt->d3dpp.BackBufferHeight      = ctxt->sh;
    ctxt->d3dpp.MultiSampleType       = D3DMULTISAMPLE_NONE;
    ctxt->d3dpp.SwapEffect            = D3DSWAPEFFECT_DISCARD;
    ctxt->d3dpp.hDeviceWindow         = (HWND)ctxt->hwnd;
    ctxt->d3dpp.Windowed              = TRUE;
    ctxt->d3dpp.EnableAutoDepthStencil= FALSE;
#if ENABLE_WAIT_D3D_VSYNC
    ctxt->d3dpp.PresentationInterval  = d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
#else
    ctxt->d3dpp.PresentationInterval  = D3DPRESENT_INTERVAL_IMMEDIATE;
#endif

    if (FAILED(ctxt->pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)ctxt->hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &ctxt->d3dpp, &ctxt->pD3DDev)) )
    {
        av_log(NULL, AV_LOG_ERROR, "failed to create d3d device !\n");
        exit(0);
    }

    //++ try pixel format
    if (SUCCEEDED(ctxt->pD3DDev->CreateOffscreenPlainSurface(1, 1, D3DFMT_YUY2,
            D3DPOOL_DEFAULT, &ctxt->pSurfs[0], NULL))) {
        ctxt->d3dfmt = D3DFMT_YUY2;
        ctxt->pixfmt = AV_PIX_FMT_YUYV422;
    }
    else if (SUCCEEDED(ctxt->pD3DDev->CreateOffscreenPlainSurface(1, 1, D3DFMT_UYVY,
            D3DPOOL_DEFAULT, &ctxt->pSurfs[0], NULL))) {
        ctxt->d3dfmt = D3DFMT_UYVY;
        ctxt->pixfmt = AV_PIX_FMT_UYVY422;
    }
    else {
        ctxt->d3dfmt = D3DFMT_X8R8G8B8;
        ctxt->pixfmt = AV_PIX_FMT_RGB32;
    }
    ctxt->pSurfs[0]->Release();
    ctxt->pSurfs[0] = NULL;
    //-- try pixel format

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}

void vdev_d3d_destroy(void *ctxt)
{
    int i;
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;

    // make visual effect & rendering thread safely exit
    c->status = VDEV_CLOSE;
    sem_post(&c->semr);
    sem_post(&c->semw);
    pthread_join(c->thread, NULL);

    for (i=0; i<c->bufnum; i++) {
        if (c->pSurfs[i]) {
            c->pSurfs[i]->Release();
        }
    }

    c->pD3DDev->Release();
    c->pD3D9  ->Release();

    // close semaphore
    sem_destroy(&c->semr);
    sem_destroy(&c->semw);

#if CLEAR_VDEV_WHEN_DESTROYED
    // clear window to background
    InvalidateRect((HWND)c->hwnd, NULL, TRUE);
#endif

    // free memory
    free(c->ppts  );
    free(c->pSurfs);
    free(c);
}

void vdev_d3d_request(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;

    sem_wait(&c->semw);
    if (c->status & VDEV_CLOSE) return;

    if (!c->pSurfs[c->tail]) {
        // create surface
        if (FAILED(c->pD3DDev->CreateOffscreenPlainSurface(c->sw, c->sh, (D3DFORMAT)c->d3dfmt,
                    D3DPOOL_DEFAULT, &c->pSurfs[c->tail], NULL)))
        {
            av_log(NULL, AV_LOG_ERROR, "failed to create d3d off screen plain surface !\n");
            exit(0);
        }
    }

    // lock texture rect
    D3DLOCKED_RECT d3d_rect;
    c->pSurfs[c->tail]->LockRect(&d3d_rect, NULL, D3DLOCK_DISCARD);

    if (buffer  ) buffer[0]   = (uint8_t*)d3d_rect.pBits;
    if (linesize) linesize[0] = d3d_rect.Pitch;
}


void vdev_d3d_post(void *ctxt, int64_t pts)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    c->pSurfs[c->tail]->UnlockRect();
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    sem_post(&c->semr);
}

void vdev_d3d_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    c->x  = x; c->y  = y;
    c->w  = w; c->h  = h;
//  c->sw = w; c->sh = h; // remove for d3d vdev
    c->refresh_flag  = 1;
}
