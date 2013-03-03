// 包含头文件
#include <windows.h>
#include <mmsystem.h>
extern "C" {
#include "corerender.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

inline void TRACE(LPCSTR lpszFormat, ...)
{
    va_list args;
    char    buf[512];

    va_start(args, lpszFormat);
    _vsnprintf(buf, sizeof(buf), lpszFormat, args);
    OutputDebugStringA(buf);
    va_end(args);
}

// 内部常量定义
#define RENDER_STOP    0
#define RENDER_PLAY    1
#define RENDER_PAUSE   2

#define WAVE_BUF_SIZE  8192
#define WAVE_BUF_NUM   32
#define VIDEO_BUF_NUM  32

// 内部类型定义
typedef struct
{
    int         nVideoWidth;
    int         nVideoHeight;
    int         nRenderPosX;
    int         nRenderPosY;
    int         nRenderWidth;
    int         nRenderHeight;
    HWND        hRenderWnd;
    SwrContext *pSWRContext;
    SwsContext *pSWSContext;
    PixelFormat PixelFormat;

    int         nVideoWrite;
    int         nVideoRead;
    int         nVideoNumCur;
    int         nVideoNumTotal;
    int         nVideoStatus;
    HANDLE      hVideoThread;
    HANDLE      hVideoSem;

    DWORD       dwCurTick;
    DWORD       dwLastTick;
    int         iFrameTick;
    int         iSleepTick;

    HDC         hRenderDC;
    HDC         hBufferDC [VIDEO_BUF_NUM];
    HBITMAP     hBufferBMP[VIDEO_BUF_NUM];
    BYTE       *pBufferBMP[VIDEO_BUF_NUM];
    int         nLineSize [VIDEO_BUF_NUM];

    HWAVEOUT    hWaveOut;
    HANDLE      hWaveSem;
    int         nWaveCur;
    WAVEHDR     wavehdr[WAVE_BUF_NUM];
    BYTE        wavebuf[WAVE_BUF_NUM][WAVE_BUF_SIZE];
} RENDER;

// 内部函数实现
static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    RENDER *render = (RENDER*)dwInstance;
    switch (uMsg)
    {
    case WOM_DONE:
        ReleaseSemaphore(render->hWaveSem, 1, NULL);
        break;
    }
}

static DWORD WINAPI VideoRenderThreadProc(RENDER *render)
{
    while (render->nVideoStatus != RENDER_STOP)
    {
        if (render->nVideoStatus == RENDER_PLAY && render->nVideoNumCur > 0)
        {
            BitBlt(render->hRenderDC, render->nRenderPosX, render->nRenderPosY,
                render->nRenderWidth, render->nRenderHeight,
                render->hBufferDC[render->nVideoRead], 0, 0, SRCCOPY);

            render->nVideoRead++;
            render->nVideoRead %= render->nVideoNumTotal;
            render->nVideoNumCur--;
            ReleaseSemaphore(render->hVideoSem, 1, NULL);

            // ++ frame rate control ++ //
            render->dwLastTick = render->dwCurTick;
            render->dwCurTick  = GetTickCount();
            if (render->dwCurTick - render->dwLastTick > (DWORD)render->iFrameTick) {
                render->iSleepTick--;
            }
            else if (render->dwCurTick - render->dwLastTick < (DWORD)render->iFrameTick) {
                render->iSleepTick++;
            }
            if (render->iSleepTick <= 0) render->iSleepTick = 1;
            // -- frame rate control -- //
        }
        Sleep(render->iSleepTick);
    }

    return TRUE;
}

// 函数实现
HANDLE renderopen(HWND hwnd, AVRational frate, int pixfmt, int w, int h,
                  int64_t chan_layout, AVSampleFormat format, int rate)
{
    WAVEFORMATEX wfx = {0};
    int          i;

    RENDER *render = (RENDER*)malloc(sizeof(RENDER));
    memset(render, 0, sizeof(RENDER));
    render->hRenderWnd = hwnd; // save hwnd

    // init for audio
    render->hWaveSem = CreateSemaphore(NULL, WAVE_BUF_NUM, WAVE_BUF_NUM, NULL);
    wfx.cbSize          = sizeof(wfx);
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.wBitsPerSample  = 16;    // 16bit
    wfx.nSamplesPerSec  = 44100; // 44.1k
    wfx.nChannels       = 2;     // stereo
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    waveOutOpen(&(render->hWaveOut), WAVE_MAPPER, &wfx, (DWORD)waveOutProc, (DWORD)render, CALLBACK_FUNCTION);
    waveOutPause(render->hWaveOut);
    for (i=0; i<WAVE_BUF_NUM; i++) {
        render->wavehdr[i].lpData         = (LPSTR)render->wavebuf[i];
        render->wavehdr[i].dwBufferLength = WAVE_BUF_SIZE;
        render->wavehdr[i].dwUser         = i;
        waveOutPrepareHeader(render->hWaveOut, render->wavehdr + i, sizeof(WAVEHDR));
    }

    /* allocate & init swr context */
    render->pSWRContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
        chan_layout, format, rate, 0, NULL);
    swr_init(render->pSWRContext);

    // init for video
    if (w == 0 || h == 0) return (HANDLE)render;
    render->nVideoWidth  = w;
    render->nVideoHeight = h;
    render->PixelFormat  = (PixelFormat)pixfmt;
    rendersetrect(render, 0, 0, w, h);

    render->iFrameTick = 1000 * frate.den / frate.num;
    render->iSleepTick = render->iFrameTick / 2;

    // create dc & bitmaps
    render->hRenderDC = GetDC(render->hRenderWnd);

    BYTE        infobuf[sizeof(BITMAPINFO) + 3 * sizeof(RGBQUAD)] = {0};
    BITMAPINFO *bmpinfo = (BITMAPINFO*)infobuf;
    BITMAP      bitmap  = {0};
    BYTE       *pbuf    = NULL;
    RECT        rect;
    GetClientRect(hwnd, &rect);
    bmpinfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmpinfo->bmiHeader.biWidth       = rect.right;
    bmpinfo->bmiHeader.biHeight      =-rect.bottom;
    bmpinfo->bmiHeader.biPlanes      = 1;
    bmpinfo->bmiHeader.biBitCount    = 16;
    bmpinfo->bmiHeader.biCompression = BI_BITFIELDS;

    DWORD *quad = (DWORD*)&(bmpinfo->bmiColors[0]);
    quad[0] = 0xF800; // RGB565
    quad[1] = 0x07E0;
    quad[2] = 0x001F;

    for (i=0; i<VIDEO_BUF_NUM; i++) {
        render->hBufferDC [i] = CreateCompatibleDC(NULL);
        render->hBufferBMP[i] = CreateDIBSection(render->hBufferDC[i], bmpinfo, DIB_RGB_COLORS, (void**)&pbuf, NULL, 0);
        if (!render->hBufferDC[i] || !render->hBufferBMP[i]) break;
        SelectObject(render->hBufferDC[i], render->hBufferBMP[i]);
        GetObject(render->hBufferBMP[i], sizeof(BITMAP), &bitmap);
        render->pBufferBMP[i] = pbuf;
        render->nLineSize [i] = bitmap.bmWidthBytes;
    }
    render->nVideoNumTotal = i;

    render->nVideoStatus = RENDER_PLAY;
    render->hVideoSem    = CreateSemaphore(NULL, render->nVideoNumTotal, render->nVideoNumTotal, NULL);
    render->hVideoThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)VideoRenderThreadProc, render, 0, NULL);
    return (HANDLE)render;
}

void renderclose(HANDLE hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;

    for (int i=0; i<WAVE_BUF_NUM; i++) {
        if (render->hWaveOut) waveOutUnprepareHeader(render->hWaveOut, render->wavehdr + i, sizeof(WAVEHDR));
    }
    if (render->hWaveOut) waveOutClose(render->hWaveOut);
    if (render->hWaveSem) CloseHandle (render->hWaveSem);

    // free swr context
    swr_free(&(render->pSWRContext));

    render->nVideoStatus = RENDER_STOP;
    ReleaseSemaphore(render->hVideoSem, 1, NULL);
    WaitForSingleObject(render->hVideoThread, -1);
    CloseHandle(render->hVideoThread);
    CloseHandle(render->hVideoSem   );

    // set zero to free sws context
    rendersetrect(render, 0, 0, 0, 0);

    for (int i=0; i<VIDEO_BUF_NUM; i++) {
        if (render->hBufferDC [i]) DeleteDC(render->hBufferDC[i]);
        if (render->hBufferBMP[i]) DeleteObject(render->hBufferBMP[i]);
    }
    if (render->hRenderDC) ReleaseDC(render->hRenderWnd, render->hRenderDC);
    free(render);
}

void renderaudiowrite(HANDLE hrender, AVFrame *audio)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    BYTE   *wavebuf;
    int     sampnum;

    WaitForSingleObject(render->hWaveSem, -1);
    wavebuf = render->wavebuf[render->nWaveCur];
    sampnum = WAVE_BUF_SIZE / 4; /* 2-channel, 16bit */
    // todo
    sampnum = swr_convert(render->pSWRContext, &wavebuf, sampnum,
        (const uint8_t**)audio->extended_data, audio->nb_samples);
    sampnum = audio->nb_samples;
    render->wavehdr[render->nWaveCur].dwBufferLength = sampnum * 4;
    waveOutWrite(render->hWaveOut, &(render->wavehdr[render->nWaveCur]), sizeof(WAVEHDR));
    render->nWaveCur++;
    render->nWaveCur %= WAVE_BUF_NUM;
}

void rendervideowrite(HANDLE hrender, AVFrame *video)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;

    // if video buffer list full please wait
    WaitForSingleObject(render->hVideoSem, -1);

    AVPicture picture = {0};
    picture.data[0]     = render->pBufferBMP[render->nVideoWrite];
    picture.linesize[0] = render->nLineSize [render->nVideoWrite];
    sws_scale(render->pSWSContext, video->data, video->linesize, 0, render->nVideoHeight, picture.data, picture.linesize);

    render->nVideoWrite++;
    render->nVideoWrite %= render->nVideoNumTotal;
    render->nVideoNumCur++;
}

void rendersetrect(HANDLE hrender, int x, int y, int w, int h)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;

    // release first
    if (render->pSWSContext)
    {
        sws_freeContext(render->pSWSContext);
        render->pSWSContext = NULL;
    }
    if (x == 0 && y == 0 && w == 0 && h == 0) return;

    render->nRenderPosX   = x;
    render->nRenderPosY   = y;
    render->nRenderWidth  = w;
    render->nRenderHeight = h;

    // recreate then
    render->pSWSContext = sws_getContext(
        render->nVideoWidth,
        render->nVideoHeight,
        render->PixelFormat,
        render->nRenderWidth,
        render->nRenderHeight,
        PIX_FMT_RGB565,
        SWS_BILINEAR,
        0, 0, 0);
}

void renderstart(HANDLE hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    waveOutRestart(render->hWaveOut);
    render->nVideoStatus = RENDER_PLAY;
}

void renderpause(HANDLE hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    waveOutPause(render->hWaveOut);
    render->nVideoStatus = RENDER_PAUSE;
}

void renderflush(HANDLE hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    render->nWaveCur     = 0;
    render->nVideoRead   = 0;
    render->nVideoWrite  = 0;
    render->nVideoNumCur = 0;
    ReleaseSemaphore(render->hWaveSem, WAVE_BUF_NUM, NULL);
    ReleaseSemaphore(render->hVideoSem, render->nVideoNumTotal, NULL);
    waveOutReset(render->hWaveOut);
}

