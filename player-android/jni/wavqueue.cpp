// 包含头文件
#include "wavqueue.h"

// 函数实现
bool wavqueue_create(WAVQUEUE *pwq)
{
    return true;
}

void wavqueue_destroy(WAVQUEUE *pwq)
{
}

bool wavqueue_isempty(WAVQUEUE *pwq)
{
    int value = 0;
    sem_getvalue(&(pwq->semr), &value);
    return (value <= 0);
}

void wavqueue_write_request(WAVQUEUE *pwq, int64_t **ppts)
{
    sem_wait(&(pwq->semw));
    if (ppts ) *ppts  = &(pwq->ppts[pwq->tail]);
}

void wavqueue_write_release(WAVQUEUE *pwq)
{
    sem_post(&(pwq->semw));
}

void wavqueue_write_done(WAVQUEUE *pwq)
{
    if (++pwq->tail == pwq->size) pwq->tail = 0;
    sem_post(&(pwq->semr));
}

void wavqueue_read_request(WAVQUEUE *pwq, int64_t **ppts)
{
    sem_wait(&(pwq->semr));
    if (ppts) *ppts = &(pwq->ppts[pwq->head]);
}

void wavqueue_read_release(WAVQUEUE *pwq)
{
    sem_post(&(pwq->semr));
}

void wavqueue_read_done(WAVQUEUE *pwq)
{
    if (++pwq->head == pwq->size) pwq->head = 0;
    sem_post(&(pwq->semw));
}





