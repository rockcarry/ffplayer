// 包含头文件
#include <pthread.h>
#include <semaphore.h>
#include "pktqueue.h"

// 内部常量定义
#define DEF_PKT_QUEUE_SIZE 256 // must be a power of 2

// 内部类型定义
typedef struct {
    int        fsize;
    int        asize;
    int        vsize;
    int        fhead;
    int        ftail;
    int        ahead;
    int        atail;
    int        vhead;
    int        vtail;
    sem_t      fsem;
    sem_t      asem;
    sem_t      vsem;
    AVPacket  *bpkts; // packet buffers
    AVPacket **fpkts; // free packets
    AVPacket **apkts; // audio packets
    AVPacket **vpkts; // video packets
} PKTQUEUE;

// 函数实现
void* pktqueue_create(int size)
{
    PKTQUEUE *ppq = (PKTQUEUE*)calloc(1, sizeof(PKTQUEUE));
    if (!ppq) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate pktqueue context !\n");
        exit(0);
    }

    ppq->fsize = size ? size : DEF_PKT_QUEUE_SIZE;
    ppq->asize = ppq->vsize = ppq->fsize;

    // alloc buffer & semaphore
    ppq->bpkts = (AVPacket* )calloc(ppq->fsize, sizeof(AVPacket ));
    ppq->fpkts = (AVPacket**)calloc(ppq->fsize, sizeof(AVPacket*));
    ppq->apkts = (AVPacket**)calloc(ppq->asize, sizeof(AVPacket*));
    ppq->vpkts = (AVPacket**)calloc(ppq->vsize, sizeof(AVPacket*));
    sem_init(&ppq->fsem, 0, ppq->fsize);
    sem_init(&ppq->asem, 0, 0         );
    sem_init(&ppq->vsem, 0, 0         );

    // check invalid
    if (!ppq->bpkts || !ppq->fpkts || !ppq->apkts || !ppq->vpkts) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for pktqueue !\n");
        exit(0);
    }

    // init fpkts
    for (int i=0; i<ppq->fsize; i++) {
        ppq->fpkts[i] = &ppq->bpkts[i];
    }

    return ppq;
}

void pktqueue_destroy(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;

    // close
    sem_destroy(&ppq->fsem);
    sem_destroy(&ppq->asem);
    sem_destroy(&ppq->vsem);

    // free
    free(ppq->bpkts);
    free(ppq->fpkts);
    free(ppq->apkts);
    free(ppq->vpkts);
    free(ppq);
}

void pktqueue_reset(void *ctxt)
{
    PKTQUEUE *ppq    = (PKTQUEUE*)ctxt;
    AVPacket *packet = NULL;

    while (NULL != (packet = pktqueue_read_request_a(ctxt))) {
        av_packet_unref(packet);
        pktqueue_read_done_a(ctxt, packet);
    }

    while (NULL != (packet = pktqueue_read_request_v(ctxt))) {
        av_packet_unref(packet);
        pktqueue_read_done_v(ctxt, packet);
    }

    ppq->fhead = ppq->ftail = 0;
    ppq->ahead = ppq->atail = 0;
    ppq->vhead = ppq->vtail = 0;
}

AVPacket* pktqueue_write_request(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    if (0 != sem_trywait(&ppq->fsem)) return NULL;
    return ppq->fpkts[ppq->fhead++ & (ppq->fsize - 1)];
}

void pktqueue_write_post_a(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->apkts[ppq->atail++ & (ppq->asize - 1)] = pkt;
    sem_post(&ppq->asem);
}

void pktqueue_write_post_v(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->vpkts[ppq->vtail++ & (ppq->vsize - 1)] = pkt;
    sem_post(&ppq->vsem);
}

void pktqueue_write_post_i(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail++ & (ppq->fsize - 1)] = pkt;
    sem_post(&ppq->fsem);
}

AVPacket* pktqueue_read_request_a(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    if (0 != sem_trywait(&ppq->asem)) return NULL;
    return ppq->apkts[ppq->ahead++ & (ppq->asize - 1)];
}

void pktqueue_read_done_a(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail++ & (ppq->fsize - 1)] = pkt;
    sem_post(&ppq->fsem);
}

AVPacket* pktqueue_read_request_v(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    if (0 != sem_trywait(&ppq->vsem)) return NULL;
    return ppq->vpkts[ppq->vhead++ & (ppq->vsize - 1)];
}

void pktqueue_read_done_v(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail++ & (ppq->fsize - 1)] = pkt;
    sem_post(&ppq->fsem);
}



