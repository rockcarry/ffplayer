// 包含头文件
#include "bmpqueue.h"

// 函数实现
bool bmpqueue_create(BMPQUEUE *pbq, sp<ANativeWindow> win, int w, int h)
{
    return true;
}

void bmpqueue_destroy(BMPQUEUE *pbq)
{
}

bool bmpqueue_isempty(BMPQUEUE *pbq)
{
    int value = 0;
    sem_getvalue(&(pbq->semr), &value);
    return (value <= 0);
}

void bmpqueue_write_request(BMPQUEUE *pbq, int64_t **ppts, uint8_t **pbuf, int *stride)
{
    sem_wait(&(pbq->semw));
}

void bmpqueue_write_release(BMPQUEUE *pbq)
{
    sem_post(&(pbq->semw));
}

void bmpqueue_write_done(BMPQUEUE *pbq)
{
    if (++pbq->tail == pbq->size) pbq->tail = 0;
    sem_post(&(pbq->semr));
}

void bmpqueue_read_request(BMPQUEUE *pbq, int64_t **ppts, ANativeWindowBuffer **buf)
{
    sem_wait(&(pbq->semr));
    if (ppts) *ppts = &(pbq->ppts[pbq->head]);
}

void bmpqueue_read_release(BMPQUEUE *pbq)
{
    sem_post(&(pbq->semr));
}

void bmpqueue_read_done(BMPQUEUE *pbq)
{
    if (++pbq->head == pbq->size) pbq->head = 0;
    sem_post(&(pbq->semw));
}





