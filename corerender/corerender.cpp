// 包含头文件
#include <windows.h>
#include <mmsystem.h>
#include "../coreplayer/coreplayer.h"
#include "corerender.h"
#include "wavbufqueue.h"
#include "bmpbufqueue.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

inline void TRACE(LPCSTR lpszFormat, ...)
{
#ifdef _DEBUG
    va_list args;
    char    buf[512];

    va_start(args, lpszFormat);
    _vsnprintf_s(buf, sizeof(buf), lpszFormat, args);
    OutputDebugStringA(buf);
    va_end(args);
#endif
}

// 内部常量定义
enum {
    RENDER_STOP,
    RENDER_PLAY,
    RENDER_PAUSE,
    RENDER_SEEK,
};

// 内部类型定义
typedef struct
{
    int         nVideoWidth;
    int         nVideoHeight;
    PixelFormat PixelFormat;
    SwrContext *pSWRContext;
    SwsContext *pSWSContext;

    HWAVEOUT    hWaveOut;
    WAVBUFQUEUE WavBufQueue;

    int         nRenderStatus;
    int         nRenderPosX;
    int         nRenderPosY;
    int         nRenderWidth;
    int         nRenderHeight;
    HWND        hRenderWnd;
    HDC         hRenderDC;
    HDC         hBufferDC;
    HANDLE      hVideoThread;
    BMPBUFQUEUE BmpBufQueue;

    DWORD       dwCurTick;
    DWORD       dwLastTick;
    int         iFrameTick;
    int         iSleepTick;
    int64_t     i64CurTimeA;
    int64_t     i64CurTimeV;
} RENDER;

// 内部函数实现
static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    RENDER  *render = (RENDER*)dwInstance;
    WAVEHDR *pwhdr  = NULL;
    int64_t *ppts   = NULL;
    switch (uMsg)
    {
    case WOM_DONE:
        wavbufqueue_read_request(&(render->WavBufQueue), &ppts, &pwhdr);
        //++ play completed ++//
        if (*ppts == -1) {
            PostMessage(render->hRenderWnd, MSG_COREPLAYER, PLAY_COMPLETED, 0);
        }
        //-- play completed --//
        else {
            render->i64CurTimeA = *ppts + 1000 * pwhdr->dwBytesRecorded / (44100 * 4);
            TRACE("i64CurTimeA = %d\n", render->i64CurTimeA);
        }
        wavbufqueue_read_done(&(render->WavBufQueue));
        break;
    }
}

static DWORD WINAPI VideoRenderThreadProc(RENDER *render)
{
    while (render->nRenderStatus != RENDER_STOP)
    {
        if (render->nRenderStatus == RENDER_PAUSE) {
            Sleep(50);
            continue;
        }

        HBITMAP hbitmap = NULL;
        int64_t *ppts   = NULL;
        bmpbufqueue_read_request(&(render->BmpBufQueue), &ppts, &hbitmap);
        if (render->nRenderStatus == RENDER_PLAY) {
            //++ play completed ++//
            if (*ppts == -1) {
                PostMessage(render->hRenderWnd, MSG_COREPLAYER, PLAY_COMPLETED, 0);
            }
            //-- play completed --//
            else {
                render->i64CurTimeV = *ppts; // the current play time
                SelectObject(render->hBufferDC, hbitmap);
                BitBlt(render->hRenderDC, render->nRenderPosX, render->nRenderPosY,
                    render->nRenderWidth, render->nRenderHeight,
                    render->hBufferDC, 0, 0, SRCCOPY);
                TRACE("i64CurTimeV = %d\n", render->i64CurTimeV);
            }
        }
        bmpbufqueue_read_done(&(render->BmpBufQueue));

        // ++ frame rate control ++ //
        render->dwLastTick = render->dwCurTick;
        render->dwCurTick  = GetTickCount();
        if (render->dwCurTick - render->dwLastTick > (DWORD)render->iFrameTick) {
            render->iSleepTick--;
        }
        else if (render->dwCurTick - render->dwLastTick < (DWORD)render->iFrameTick) {
            render->iSleepTick++;
        }
        // -- frame rate control -- //

        // ++ av sync control ++ //
        int64_t curdt = render->i64CurTimeV - render->i64CurTimeA;
        if (curdt >  100) render->iSleepTick++;
        if (curdt < -100) render->iSleepTick--;
        // -- av sync control -- //

        if (render->iSleepTick > 0) Sleep(render->iSleepTick);
    }

    return 0;
}

// 函数实现
HANDLE renderopen(HWND hwnd, AVRational frate, int pixfmt, int w, int h,
                  int64_t chan_layout, AVSampleFormat format, int rate)
{
    WAVEFORMATEX wfx = {0};

    RENDER *render = (RENDER*)malloc(sizeof(RENDER));
    memset(render, 0, sizeof(RENDER));
    render->hRenderWnd = hwnd; // save hwnd

    // init for audio
    wfx.cbSize          = sizeof(wfx);
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.wBitsPerSample  = 16;    // 16bit
    wfx.nSamplesPerSec  = 44100; // 44.1k
    wfx.nChannels       = 2;     // stereo
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    waveOutOpen(&(render->hWaveOut), WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD)render, CALLBACK_FUNCTION);
    waveOutPause(render->hWaveOut);
    wavbufqueue_create(&(render->WavBufQueue), render->hWaveOut);

    /* allocate & init swr context */
    render->pSWRContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
        chan_layout, format, rate, 0, NULL);
    swr_init(render->pSWRContext);

    // init for video
    render->nVideoWidth  = w;
    render->nVideoHeight = h;
    render->PixelFormat  = (PixelFormat)pixfmt;
    rendersetrect(render, 0, 0, w, h);

    render->iFrameTick = 1000 * frate.den / frate.num;
    render->iSleepTick = render->iFrameTick / 2;

    // create dc & bitmaps
    render->hRenderDC = GetDC(render->hRenderWnd);
    render->hBufferDC = CreateCompatibleDC(NULL);

    RECT rect = {0};
    GetClientRect(hwnd, &rect);
    bmpbufqueue_create(&(render->BmpBufQueue), render->hBufferDC, rect.right, rect.bottom, 16);

    render->nRenderStatus = RENDER_PLAY;
    render->hVideoThread  = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)VideoRenderThreadProc, render, 0, NULL);
    return (HANDLE)render;
}

void renderclose(HANDLE hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;

    //++ audio ++//
    // destroy wave buffer queue
    wavbufqueue_destroy(&(render->WavBufQueue));

    // wave out close
    if (render->hWaveOut) waveOutClose(render->hWaveOut);

    // free swr context
    swr_free(&(render->pSWRContext));
    //-- audio --//

    //++ video ++//
    // set nRenderStatus to RENDER_STOP
    render->nRenderStatus = RENDER_STOP;
    if (bmpbufqueue_isempty(&(render->BmpBufQueue))) {
        bmpbufqueue_write_request(&(render->BmpBufQueue), NULL, NULL, NULL);
        bmpbufqueue_write_done   (&(render->BmpBufQueue));
    }
    WaitForSingleObject(render->hVideoThread, -1);
    CloseHandle(render->hVideoThread);

    // set zero to free sws context
    rendersetrect(render, 0, 0, 0, 0);

    if (render->hBufferDC) DeleteDC (render->hBufferDC);
    if (render->hRenderDC) ReleaseDC(render->hRenderWnd, render->hRenderDC);

    // destroy bmp buffer queue
    bmpbufqueue_destroy(&(render->BmpBufQueue));
    //-- video --//

    // free context
    free(render);
}

void renderaudiowrite(HANDLE hrender, AVFrame *audio)
{
    if (!hrender) return;
    RENDER  *render = (RENDER*)hrender;
    WAVEHDR *wavehdr;
    int      sampnum;
    int64_t *ppts;

    if (render->nRenderStatus == RENDER_SEEK) return;

    //++ play completed ++//
    if (audio == (AVFrame*)-1) {
        wavbufqueue_write_request(&(render->WavBufQueue), &ppts, &wavehdr);
        *ppts = -1; // *ppts == -1, means completed
        memset(wavehdr->lpData, 0, wavehdr->dwUser);
        wavehdr->dwBufferLength = (DWORD)wavehdr->dwUser;
        waveOutWrite(render->hWaveOut, wavehdr, sizeof(WAVEHDR));
        wavbufqueue_write_done(&(render->WavBufQueue));
        return;
    }
    //-- play completed --//

    do {
        wavbufqueue_write_request(&(render->WavBufQueue), &ppts, &wavehdr);

        // record audio pts
        *ppts = audio->pts;

        //++ do resample audio data ++//
        sampnum = swr_convert(render->pSWRContext, (uint8_t **)&(wavehdr->lpData),
            (int)wavehdr->dwUser / 4, (const uint8_t**)audio->extended_data,
            audio->nb_samples);
        audio->extended_data = NULL;
        audio->nb_samples    = 0;
        //-- do resample audio data --//

        //++ post or release waveout audio buffer ++//
        if (sampnum > 0) {
            wavehdr->dwBufferLength = sampnum * 4;
            waveOutWrite(render->hWaveOut, wavehdr, sizeof(WAVEHDR));
            wavbufqueue_write_done(&(render->WavBufQueue));
        }
        else {
            // we must release semaphore
            wavbufqueue_write_release(&(render->WavBufQueue));
        }
        //-- post or release waveout audio buffer --//
    } while (sampnum > 0);
}

void rendervideowrite(HANDLE hrender, AVFrame *video)
{
    if (!hrender) return;
    RENDER   *render  = (RENDER*)hrender;
    AVPicture picture = {0};
    BYTE     *bmpbuf  = NULL;
    int       stride  = 0;
    int64_t  *ppts    = NULL;

    if (render->nRenderStatus == RENDER_SEEK) return;
    bmpbufqueue_write_request(&(render->BmpBufQueue), &ppts, &bmpbuf, &stride);
    //++ play completed ++//
    if (video == (AVFrame*)-1) *ppts = -1;
    //-- play completed --//
    else {
        *ppts               = video->pts;
        picture.data[0]     = bmpbuf;
        picture.linesize[0] = stride;
        sws_scale(render->pSWSContext, video->data, video->linesize, 0, render->nVideoHeight, picture.data, picture.linesize);
    }
    bmpbufqueue_write_done(&(render->BmpBufQueue));
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
    render->nRenderStatus = RENDER_PLAY;
}

void renderpause(HANDLE hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    waveOutPause(render->hWaveOut);
    render->nRenderStatus = RENDER_PAUSE;
}

void renderflush(HANDLE hrender, DWORD time)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;

    render->nRenderStatus = RENDER_SEEK;
    waveOutReset(render->hWaveOut); // wave out reset
    while (!wavbufqueue_isempty(&(render->WavBufQueue))) Sleep(50);
    while (!bmpbufqueue_isempty(&(render->BmpBufQueue))) Sleep(50);
    render->i64CurTimeA   = time * 1000;
    render->i64CurTimeV   = time * 1000;
    render->nRenderStatus = RENDER_PLAY;
}

void renderplaytime(HANDLE hrender, DWORD *time)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    if (time) {
        DWORD atime = render->i64CurTimeA > 0 ? (DWORD)(render->i64CurTimeA / 1000) : 0;
        DWORD vtime = render->i64CurTimeV > 0 ? (DWORD)(render->i64CurTimeV / 1000) : 0;
        *time = atime > vtime ? atime : vtime;
    }
}

