// 包含头文件
#include "adev.h"

#pragma warning(disable:4311)
#pragma warning(disable:4312)

// 内部常量定义
#define DEF_ADEV_BUF_NUM  8
#define DEF_ADEV_BUF_LEN  8192

#define SW_VOLUME_MINDB  -30
#define SW_VOLUME_MAXDB  +12

// 内部类型定义
typedef struct
{
    // waveout
    HWAVEOUT hWaveOut;
    WAVEHDR *pWaveHdr;

    int64_t *ppts;
    int      bufnum;
    int      buflen;
    int      head;
    int      tail;
    HANDLE   bufsem;
    int64_t *apts;

    // store current audio data
    SHORT   *curdata;

    // software volume
    int      vol_scaler[256];
    int      vol_zerodb;
    int      vol_curvol;
} ADEV_CONTEXT;

// 内部函数实现
static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)dwInstance;
    switch (uMsg)
    {
    case WOM_DONE:
        memcpy(c->curdata, c->pWaveHdr[c->head].lpData, c->buflen);
        if (c->apts) *c->apts = c->ppts[c->head];
        av_log(NULL, AV_LOG_DEBUG, "apts: %lld\n", *c->apts);
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->bufsem, 1, NULL);
        break;
    }
}

static int init_software_volmue_scaler(int *scaler, int mindb, int maxdb)
{
    double a[256];
    double b[256];
    int    z, i;

    for (i=0; i<256; i++) {
        a[i]      = mindb + (maxdb - mindb) * i / 256.0;
        b[i]      = pow(10.0, a[i] / 20.0);
        scaler[i] = (int)(0x10000 * b[i]);
    }

    z = -mindb * 256 / (maxdb - mindb);
    z = z > 0   ? z : 0  ;
    z = z < 255 ? z : 255;
    scaler[0] = 0;        // mute
    scaler[z] = 0x10000;  // 0db
    return z;
}

// 接口函数实现
void* adev_create(int type, int bufnum, int buflen)
{
    ADEV_CONTEXT *ctxt = NULL;
    WAVEFORMATEX  wfx  = {0};
    BYTE         *pwavbuf;
    MMRESULT      result;
    int           i;

    // allocate adev context
    ctxt = (ADEV_CONTEXT*)calloc(1, sizeof(ADEV_CONTEXT));
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate adev context !\n");
        exit(0);
    }

    bufnum         = bufnum ? bufnum : DEF_ADEV_BUF_NUM;
    buflen         = buflen ? buflen : DEF_ADEV_BUF_LEN;
    ctxt->bufnum   = bufnum;
    ctxt->buflen   = buflen;
    ctxt->head     = 0;
    ctxt->tail     = 0;
    ctxt->ppts     = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->pWaveHdr = (WAVEHDR*)calloc(bufnum, (sizeof(WAVEHDR) + buflen));
    ctxt->bufsem   = CreateSemaphore(NULL, bufnum, bufnum, NULL);
    ctxt->curdata  = (SHORT  *)calloc(1, buflen);
    if (!ctxt->ppts || !ctxt->pWaveHdr || !ctxt->bufsem || !ctxt->curdata) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate waveout buffer and waveout semaphore !\n");
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
    result = waveOutOpen(&ctxt->hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)ctxt, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        CloseHandle(ctxt->bufsem);
        free(ctxt->ppts    );
        free(ctxt->pWaveHdr);
        free(ctxt->curdata );
        free(ctxt);
        return NULL;
    }

    // init wavebuf
    pwavbuf = (BYTE*)(ctxt->pWaveHdr + bufnum);
    for (i=0; i<bufnum; i++) {
        ctxt->pWaveHdr[i].lpData         = (LPSTR)(pwavbuf + i * buflen);
        ctxt->pWaveHdr[i].dwBufferLength = buflen;
        waveOutPrepareHeader(ctxt->hWaveOut, &ctxt->pWaveHdr[i], sizeof(WAVEHDR));
    }

    // init software volume scaler
    ctxt->vol_zerodb = init_software_volmue_scaler(ctxt->vol_scaler, SW_VOLUME_MINDB, SW_VOLUME_MAXDB);
    ctxt->vol_curvol = ctxt->vol_zerodb;
    return ctxt;
}

void adev_destroy(void *ctxt)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    int i;

    // before close wavout, we need reset it first,
    // otherwise it will cause crash on vs2013
    waveOutReset(c->hWaveOut);

    // unprepare wave header & close waveout device
    for (i=0; i<c->bufnum; i++) {
        if (c->hWaveOut) {
            waveOutUnprepareHeader(c->hWaveOut, &c->pWaveHdr[i], sizeof(WAVEHDR));
        }
    }
    waveOutClose(c->hWaveOut);

    // close semaphore
    CloseHandle(c->bufsem);

    // free memory
    free(c->ppts    );
    free(c->pWaveHdr);
    free(c->curdata );
    free(c);
}

void adev_request(void *ctxt, AUDIOBUF **ppab)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    WaitForSingleObject(c->bufsem, -1);
    *ppab = (AUDIOBUF*)&c->pWaveHdr[c->tail];
}

void adev_post(void *ctxt, int64_t pts)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    c->ppts[c->tail] = pts;

    //++ software volume scale
    int    multiplier = c->vol_scaler[c->vol_curvol];
    SHORT *buf        = (SHORT*)c->pWaveHdr[c->tail].lpData;
    int    n          = c->pWaveHdr[c->tail].dwBufferLength / sizeof(SHORT);
    if (multiplier > 0x10000) {
        int64_t v;
        while (n--) {
            v = ((int64_t)*buf * multiplier) >> 16;
            v = v < 0x7fff ? v : 0x7fff;
            v = v >-0x7fff ? v :-0x7fff;
            *buf++ = (SHORT)v;
        }
    }
    else if (multiplier < 0x10000) {
        while (n--) {
            *buf = ((int32_t)*buf * multiplier) >> 16; buf++;
        }
    }
    //-- software volume scale

    waveOutWrite(c->hWaveOut, &c->pWaveHdr[c->tail], sizeof(WAVEHDR));
    if (++c->tail == c->bufnum) c->tail = 0;
}

void adev_pause(void *ctxt, int pause)
{
    if (!ctxt) return;
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
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    waveOutReset(c->hWaveOut);
    c->head = c->tail = 0;
    ReleaseSemaphore(c->bufsem, c->bufnum, NULL);
}

void adev_syncapts(void *ctxt, int64_t *apts)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    c->apts = apts;
    if (c->apts) {
        *c->apts = -1;
    }
}

void adev_curdata(void *ctxt, void **buf, int *len)
{
    if (!ctxt) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;
    if (buf) *buf = c->curdata;
    if (len) *len = c->buflen;
}

void adev_setparam(void *ctxt, int id, void *param)
{
    if (!ctxt || !param) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;

    switch (id) {
    case PARAM_AUDIO_VOLUME:
        {
            int vol = *(int*)param;
            vol += c->vol_zerodb;
            vol  = vol > 0   ? vol : 0  ;
            vol  = vol < 255 ? vol : 255;
            c->vol_curvol = vol;
        }
        break;
    }
}

void adev_getparam(void *ctxt, int id, void *param)
{
    if (!ctxt || !param) return;
    ADEV_CONTEXT *c = (ADEV_CONTEXT*)ctxt;

    switch (id) {
    case PARAM_AUDIO_VOLUME:
        *(int*)param = c->vol_curvol - c->vol_zerodb;
        break;
    }
}

