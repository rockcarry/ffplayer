// 包含头文件
#include "adev.h"
#include "log.h"

#pragma warning(disable:4311)
#pragma warning(disable:4312)

// 内部常量定义
#define DEF_ADEV_BUF_NUM  5
#define DEF_ADEV_BUF_LEN  8192

// 内部类型定义
typedef struct
{
    HWAVEOUT hWaveOut;
    WAVEHDR *pWaveHdr;
    int64_t *ppts;
    int      bufnum;
    int      buflen;
    int      head;
    int      tail;
    HANDLE   bufsem;
    int64_t *apts;
} ADEV_CONTEXT;

// 内部函数实现
static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)dwInstance;
    switch (uMsg)
    {
    case WOM_DONE:
        if (c->apts) *c->apts = c->ppts[c->head];
//      log_printf(TEXT("apts = %lld\n"), *c->apts);
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->bufsem, 1, NULL);
        break;
    }
}

// 接口函数实现
void* adev_create(int bufnum, int buflen)
{
    ADEV_CONTEXT *ctxt = NULL;
    WAVEFORMATEX  wfx  = {0};
    BYTE         *pwavbuf;
    int           i;

    // allocate adev context
    ctxt = (ADEV_CONTEXT*)malloc(sizeof(ADEV_CONTEXT));
    if (!ctxt) {
        log_printf(TEXT("failed to allocate adev context !\n"));
        exit(0);
    }

    bufnum         = bufnum ? bufnum : DEF_ADEV_BUF_NUM;
    buflen         = buflen ? buflen : DEF_ADEV_BUF_LEN;
    ctxt->bufnum   = bufnum;
    ctxt->buflen   = buflen;
    ctxt->head     = 0;
    ctxt->tail     = 0;
    ctxt->ppts     = (int64_t*)malloc(bufnum * sizeof(int64_t));
    ctxt->pWaveHdr = (WAVEHDR*)malloc(bufnum * (sizeof(WAVEHDR) + buflen));
    ctxt->bufsem   = CreateSemaphore(NULL, bufnum, bufnum, NULL);
    if (!ctxt->ppts || !ctxt->pWaveHdr || !ctxt->bufsem) {
        log_printf(TEXT("failed to allocate waveout buffer and waveout semaphore !\n"));
        exit(0);
    }

    // init for audio
    wfx.cbSize          = sizeof(wfx);
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.wBitsPerSample  = 16;    // 16bit
    wfx.nSamplesPerSec  = 44100; // 44.1k
    wfx.nChannels       = 2;     // stereo
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    waveOutOpen(&ctxt->hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)ctxt, CALLBACK_FUNCTION);

    // init wavebuf
    memset(ctxt->ppts    , 0, bufnum * sizeof(int64_t));
    memset(ctxt->pWaveHdr, 0, bufnum * (sizeof(WAVEHDR) + buflen));
    pwavbuf = (BYTE*)(ctxt->pWaveHdr + bufnum);
    for (i=0; i<bufnum; i++) {
        ctxt->pWaveHdr[i].lpData         = (LPSTR)(pwavbuf + i * buflen);
        ctxt->pWaveHdr[i].dwBufferLength = buflen;
        waveOutPrepareHeader(ctxt->hWaveOut, &ctxt->pWaveHdr[i], sizeof(WAVEHDR));
    }

    return ctxt;
}

void adev_destroy(void *ctxt)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    int i;

    // unprepare wave header & close waveout device
    for (i=0; i<c->bufnum; i++) {
        waveOutUnprepareHeader(c->hWaveOut, &c->pWaveHdr[i], sizeof(WAVEHDR));
    }
    waveOutClose(c->hWaveOut);

    // close semaphore
    CloseHandle(c->bufsem);

    // free memory
    free(c->ppts    );
    free(c->pWaveHdr);
    free(c);
}

void adev_request(void *ctxt, AUDIOBUF **ppab)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    WaitForSingleObject(c->bufsem, -1);
    *ppab = (AUDIOBUF*)&c->pWaveHdr[c->tail];
}

void adev_post(void *ctxt, int64_t pts)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    c->ppts[c->tail] = pts;
    waveOutWrite(c->hWaveOut, &c->pWaveHdr[c->tail], sizeof(WAVEHDR));
    if (++c->tail == c->bufnum) c->tail = 0;
}

void adev_pause(void *ctxt, BOOL pause)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    if (pause) {
        waveOutPause(c->hWaveOut);
    }
    else {
        waveOutRestart(c->hWaveOut);
    }
}

void adev_reset(void *ctxt)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    waveOutReset(c->hWaveOut);
    c->head = c->tail = 0;
    ReleaseSemaphore(c->bufsem, c->bufnum, NULL);
}

void adev_syncapts(void *ctxt, int64_t *apts)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    c->apts = apts;
}
