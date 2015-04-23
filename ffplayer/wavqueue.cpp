// 包含头文件
#include <windows.h>
#include "wavqueue.h"

// 函数实现
BOOL wavqueue_create(WAVQUEUE *pwq, HWAVEOUT h, int wavbufsize)
{
    BYTE *pwavbuf;
    int   i;

    // default size
    if (pwq->size  == 0) pwq->size  = DEF_WAV_QUEUE_SIZE;
    if (wavbufsize == 0) wavbufsize = DEF_WAV_BUFFER_SIZE;

    // alloc buffer & semaphore
    pwq->ppts    = (int64_t*)malloc(pwq->size * sizeof(int64_t));
    pwq->pwhdrs  = (WAVEHDR*)malloc(pwq->size * (sizeof(WAVEHDR) + wavbufsize));
    pwq->semr    = CreateSemaphore(NULL, 0        , pwq->size, NULL);
    pwq->semw    = CreateSemaphore(NULL, pwq->size, pwq->size, NULL);
    pwq->hwavout = h;

    // check invalid
    if (!pwq->ppts || !pwq->pwhdrs || !pwq->semr || !pwq->semw) {
        wavqueue_destroy(pwq);
        return FALSE;
    }

    // clear
    memset(pwq->ppts  , 0, pwq->size * sizeof(int64_t));
    memset(pwq->pwhdrs, 0, pwq->size * sizeof(WAVEHDR));

    // init
    pwavbuf = (BYTE*)(pwq->pwhdrs + pwq->size);
    for (i=0; i<pwq->size; i++) {
        pwq->pwhdrs[i].lpData         = (LPSTR)(pwavbuf + i * wavbufsize);
        pwq->pwhdrs[i].dwBufferLength = wavbufsize;
        pwq->pwhdrs[i].dwUser         = wavbufsize;
        waveOutPrepareHeader(pwq->hwavout, &(pwq->pwhdrs[i]), sizeof(WAVEHDR));
    }
    return TRUE;
}

void wavqueue_destroy(WAVQUEUE *pwq)
{
    int i;

    // unprepare
    for (i=0; i<pwq->size; i++) {
        waveOutUnprepareHeader(pwq->hwavout, &(pwq->pwhdrs[i]), sizeof(WAVEHDR));
    }

    if (pwq->ppts  ) free(pwq->ppts  );
    if (pwq->pwhdrs) free(pwq->pwhdrs);
    if (pwq->semr  ) CloseHandle(pwq->semr);
    if (pwq->semw  ) CloseHandle(pwq->semw);

    // clear members
    memset(pwq, 0, sizeof(WAVQUEUE));
}

BOOL wavqueue_isempty(WAVQUEUE *pwq)
{
    return (pwq->curnum <= 0);
}

void wavqueue_write_request(WAVQUEUE *pwq, int64_t **ppts, PWAVEHDR *pwhdr)
{
    WaitForSingleObject(pwq->semw, -1);
    if (ppts ) *ppts  = &(pwq->ppts[pwq->tail]);
    if (pwhdr) *pwhdr = &(pwq->pwhdrs[pwq->tail]);
}

void wavqueue_write_release(WAVQUEUE *pwq)
{
    ReleaseSemaphore(pwq->semw, 1, NULL);
}

void wavqueue_write_done(WAVQUEUE *pwq)
{
    InterlockedIncrement(&(pwq->tail));
    InterlockedCompareExchange(&(pwq->tail), 0, pwq->size);
    InterlockedIncrement(&(pwq->curnum));
    ReleaseSemaphore(pwq->semr, 1, NULL);
}

void wavqueue_read_request(WAVQUEUE *pwq, int64_t **ppts, PWAVEHDR *pwhdr)
{
    WaitForSingleObject(pwq->semr, -1);
    if (ppts ) *ppts  = &(pwq->ppts[pwq->head]);
    if (pwhdr) *pwhdr = &(pwq->pwhdrs[pwq->head]);
}

void wavqueue_read_release(WAVQUEUE *pwq)
{
    ReleaseSemaphore(pwq->semr, 1, NULL);
}

void wavqueue_read_done(WAVQUEUE *pwq)
{
    InterlockedIncrement(&(pwq->head));
    InterlockedCompareExchange(&(pwq->head), 0, pwq->size);
    InterlockedDecrement(&(pwq->curnum));
    ReleaseSemaphore(pwq->semw, 1, NULL);
}





