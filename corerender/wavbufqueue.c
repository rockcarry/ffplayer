// 包含头文件
#include <windows.h>
#include "wavbufqueue.h"

// 函数实现
BOOL wavbufqueue_create(WAVBUFQUEUE *pwq, HWAVEOUT h)
{
    BYTE *pwavbuf;
    int   i;

    // default size
    if (pwq->size == 0) pwq->size = DEF_WAVBUF_QUEUE_SIZE;

    // alloc buffer & semaphore
    pwq->pwhdrs  = malloc(pwq->size * (sizeof(WAVEHDR) + DEF_WAVBUF_BUFFER_SIZE));
    pwq->semr    = CreateSemaphore(NULL, 0        , pwq->size, NULL);
    pwq->semw    = CreateSemaphore(NULL, pwq->size, pwq->size, NULL);
    pwq->hwavout = h;

    // check invalid
    if (!pwq->pwhdrs || !pwq->semr || !pwq->semw) {
        wavbufqueue_destroy(pwq);
        return FALSE;
    }

    // clear wave headers
    memset(pwq->pwhdrs, 0, pwq->size * sizeof(WAVEHDR));

    // init
    pwavbuf = (BYTE*)(pwq->pwhdrs + pwq->size);
    for (i=0; i<pwq->size; i++) {
        pwq->pwhdrs[i].lpData         = (pwavbuf + i * DEF_WAVBUF_BUFFER_SIZE);
        pwq->pwhdrs[i].dwBufferLength = DEF_WAVBUF_BUFFER_SIZE;
        pwq->pwhdrs[i].dwUser         = DEF_WAVBUF_BUFFER_SIZE;
        waveOutPrepareHeader(pwq->hwavout, &(pwq->pwhdrs[i]), sizeof(WAVEHDR));
    }
    return TRUE;
}

void wavbufqueue_destroy(WAVBUFQUEUE *pwq)
{
    int i;

    // unprepare
    for (i=0; i<pwq->size; i++) {
        waveOutUnprepareHeader(pwq->hwavout, &(pwq->pwhdrs[i]), sizeof(WAVEHDR));
    }

    if (pwq->pwhdrs) free(pwq->pwhdrs);
    if (pwq->semr  ) CloseHandle(pwq->semr);
    if (pwq->semw  ) CloseHandle(pwq->semw);

    // clear members
    memset(pwq, 0, sizeof(WAVBUFQUEUE));
}

void wavbufqueue_clear(WAVBUFQUEUE *pwq)
{
    while (pwq->curnum > 0) {
        wavbufqueue_read_request(pwq, NULL);
        wavbufqueue_read_done(pwq);
    }
}

void wavbufqueue_write_request(WAVBUFQUEUE *pwq, PWAVEHDR *pwhdr)
{
    WaitForSingleObject(pwq->semw, -1);
    if (pwhdr) *(WAVEHDR**)pwhdr = &(pwq->pwhdrs[pwq->tail]);
}

void wavbufqueue_write_release(WAVBUFQUEUE *pwq)
{
    ReleaseSemaphore(pwq->semw, 1, NULL);
}

void wavbufqueue_write_done(WAVBUFQUEUE *pwq)
{
    InterlockedIncrement(&(pwq->tail));
    InterlockedCompareExchange(&(pwq->tail), 0, pwq->size);
    InterlockedIncrement(&(pwq->curnum));
    ReleaseSemaphore(pwq->semr, 1, NULL);
}

void wavbufqueue_read_request(WAVBUFQUEUE *pwq, PWAVEHDR *pwhdr)
{
    WaitForSingleObject(pwq->semr, -1);
    if (pwhdr) *(WAVEHDR**)pwhdr = &(pwq->pwhdrs[pwq->head]);
}

void wavbufqueue_read_release(WAVBUFQUEUE *pwq)
{
    ReleaseSemaphore(pwq->semr, 1, NULL);
}

void wavbufqueue_read_done(WAVBUFQUEUE *pwq)
{
    InterlockedIncrement(&(pwq->head));
    InterlockedCompareExchange(&(pwq->head), 0, pwq->size);
    InterlockedDecrement(&(pwq->curnum));
    ReleaseSemaphore(pwq->semw, 1, NULL);
}





