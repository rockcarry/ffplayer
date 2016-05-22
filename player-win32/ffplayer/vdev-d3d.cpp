// 包含头文件
#include <d3d9.h>
#include "vdev.h"
#include "log.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 预编译开关
#define ENABLE_WAIT_D3D_VSYNC  TRUE

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

static DWORD WINAPI VideoRenderThreadProc(void *param)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)param;

    while (!(c->nStatus & VDEV_CLOSE))
    {
        int ret = WaitForSingleObject(c->semr, c->tickframe);
        if (ret != WAIT_OBJECT_0) continue;

        if (c->refresh_flag) {
            c->refresh_flag = 0;
            vdev_refresh_background(c);
        }

        int64_t apts = c->apts;
        int64_t vpts = c->vpts = c->ppts[c->head];
#if CLEAR_VDEV_WHEN_COMPLETED
        if (vpts != -1 && !(c->nStatus & VDEV_COMPLETED)) {
#else
        if (vpts != -1) {
#endif
            d3d_draw_surf(c, c->pSurfs[c->head]);
        }

//      log_printf(TEXT("vpts: %lld\n"), vpts);
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->semw, 1, NULL);

        if (!(c->nStatus & VDEV_PAUSE)) {
            // send play progress event
            vdev_player_event(c, PLAY_PROGRESS, c->vpts > c->apts ? c->vpts : c->apts);

            //++ play completed ++//
            if (c->completed_apts != c->apts || c->completed_vpts != c->vpts) {
                c->completed_apts = c->apts;
                c->completed_vpts = c->vpts;
                c->completed_counter = 0;
            }
            else if (!(c->nStatus & VDEV_PAUSE) && ++c->completed_counter == 50) {
                log_printf(TEXT("play completed !\n"));
                c->nStatus |= VDEV_COMPLETED;
                vdev_player_event(c, PLAY_COMPLETED, 0);

#if CLEAR_VDEV_WHEN_COMPLETED
                InvalidateRect(c->hwnd, NULL, TRUE);
#endif
            }
            //-- play completed --//

            //++ frame rate & av sync control ++//
            DWORD tickcur  = GetTickCount();
            int   tickdiff = tickcur - c->ticklast;
            c->ticklast = tickcur;
            if (tickdiff - c->tickframe >  2) c->ticksleep--;
            if (tickdiff - c->tickframe < -2) c->ticksleep++;
            if (apts != -1 && vpts != -1) {
                if (apts - vpts >  5) c->ticksleep-=2;
                if (apts - vpts < -5) c->ticksleep+=2;
            }
            if (c->ticksleep > 0) Sleep(c->ticksleep);
            log_printf(TEXT("d: %3lld, s: %d\n"), apts-vpts, c->ticksleep);
            //-- frame rate & av sync control --//
        }
        else Sleep(c->tickframe);
    }

    return NULL;
}

// 接口函数实现
void* vdev_d3d_create(void *surface, int bufnum, int w, int h, int frate)
{
    VDEVD3DCTXT *ctxt = (VDEVD3DCTXT*)malloc(sizeof(VDEVD3DCTXT));
    if (!ctxt) {
        log_printf(TEXT("failed to allocate d3d vdev context !\n"));
        exit(0);
    }

    // init vdev context
    memset(ctxt, 0, sizeof(VDEVD3DCTXT));
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->hwnd      = (HWND)surface;
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
    ctxt->ppts   = (int64_t*)malloc(bufnum * sizeof(int64_t));
    ctxt->pSurfs = (LPDIRECT3DSURFACE9*)malloc(bufnum * sizeof(LPDIRECT3DSURFACE9));

    memset(ctxt->ppts  , 0, bufnum * sizeof(int64_t));
    memset(ctxt->pSurfs, 0, bufnum * sizeof(LPDIRECT3DSURFACE9));

    ctxt->semr = CreateSemaphore(NULL, 0     , bufnum, NULL);
    ctxt->semw = CreateSemaphore(NULL, bufnum, bufnum, NULL);

    // create d3d
    ctxt->pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!ctxt->ppts || !ctxt->pSurfs || !ctxt->semr || !ctxt->semw || !ctxt->pD3D9) {
        log_printf(TEXT("failed to allocate resources for vdev-d3d !\n"));
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
    ctxt->d3dpp.hDeviceWindow         = ctxt->hwnd;
    ctxt->d3dpp.Windowed              = TRUE;
    ctxt->d3dpp.EnableAutoDepthStencil= FALSE;
#if ENABLE_WAIT_D3D_VSYNC
    ctxt->d3dpp.PresentationInterval  = d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
#else
    ctxt->d3dpp.PresentationInterval  = D3DPRESENT_INTERVAL_IMMEDIATE;
#endif

    if (FAILED(ctxt->pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ctxt->hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &ctxt->d3dpp, &ctxt->pD3DDev)) )
    {
        log_printf(TEXT("failed to create d3d device !\n"));
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
    ctxt->hThread = CreateThread(NULL, 0, VideoRenderThreadProc, ctxt, 0, NULL);
    return ctxt;
}

void vdev_d3d_destroy(void *ctxt)
{
    int i;
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    // make rendering thread safely exit
    c->nStatus = VDEV_CLOSE;
    WaitForSingleObject(c->hThread, -1);
    CloseHandle(c->hThread);

    for (i=0; i<c->bufnum; i++) {
        if (c->pSurfs[i]) {
            c->pSurfs[i]->Release();
        }
    }

    c->pD3DDev->Release();
    c->pD3D9  ->Release();

    // close semaphore
    CloseHandle(c->semr);
    CloseHandle(c->semw);

#if CLEAR_VDEV_WHEN_DESTROYED
    // clear window to background
    InvalidateRect(c->hwnd, NULL, TRUE);
#endif

    // free memory
    free(c->ppts  );
    free(c->pSurfs);
    free(c);
}

void vdev_d3d_request(void *ctxt, void **buf, int *stride)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;

    WaitForSingleObject(c->semw, -1);

    if (!c->pSurfs[c->tail]) {
        // create surface
        if (FAILED(c->pD3DDev->CreateOffscreenPlainSurface(c->sw, c->sh, (D3DFORMAT)c->d3dfmt,
                    D3DPOOL_DEFAULT, &c->pSurfs[c->tail], NULL)))
        {
            log_printf(TEXT("failed to create d3d off screen plain surface !\n"));
            exit(0);
        }
    }

    // lock texture rect
    D3DLOCKED_RECT d3d_rect;
    c->pSurfs[c->tail]->LockRect(&d3d_rect, NULL, D3DLOCK_DISCARD);

    if (buf   ) *buf    = d3d_rect.pBits;
    if (stride) *stride = d3d_rect.Pitch;
}


void vdev_d3d_post(void *ctxt, int64_t pts)
{
    VDEVD3DCTXT *c = (VDEVD3DCTXT*)ctxt;
    c->pSurfs[c->tail]->UnlockRect();
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    ReleaseSemaphore(c->semr, 1, NULL);
}

void vdev_d3d_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    c->x = x; c->y = y;
    c->w = w; c->h = h;
    c->refresh_flag= 1;
}
