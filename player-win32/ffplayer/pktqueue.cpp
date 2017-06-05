// 包含头文件
#include <pthread.h>
#include <semaphore.h>
#include "pktqueue.h"

// 内部常量定义
#define DEF_PKT_QUEUE_SIZE 256

// 内部类型定义
typedef struct {
    long       fsize;
    long       asize;
    long       vsize;
    long       fhead;
    long       ftail;
    long       ahead;
    long       atail;
    long       vhead;
    long       vtail;
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
    if (  !ppq->bpkts || !ppq->fpkts || !ppq->apkts || !ppq->vpkts
       || !ppq->fsem  || !ppq->asem  || !ppq->vsem )
    {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate resources for pktqueue !\n");
        exit(0);
    }

    // init fpkts
    int i;
    for (i=0; i<ppq->fsize; i++) {
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

    while (packet = pktqueue_read_request_a(ctxt)) {
        av_packet_unref(packet);
        pktqueue_read_done_a(ctxt, packet);
    }

    while (packet = pktqueue_read_request_v(ctxt)) {
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
    AVPacket *pkt = NULL;
    if (0 != sem_trywait(&ppq->fsem)) return NULL;
    pkt = ppq->fpkts[ppq->fhead++];
    if (ppq->fhead == ppq->fsize) ppq->fhead = 0;
    return pkt;
}

void pktqueue_write_post_a(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->apkts[ppq->atail++] = pkt;
    if (ppq->atail == ppq->asize) ppq->atail = 0;
    sem_post(&ppq->asem);
}

void pktqueue_write_post_v(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->vpkts[ppq->vtail++] = pkt;
    if (ppq->vtail == ppq->vsize) ppq->vtail = 0;
    sem_post(&ppq->vsem);
}

void pktqueue_write_post_i(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail++] = pkt;
    if (ppq->ftail == ppq->fsize) ppq->ftail = 0;
    sem_post(&ppq->fsem);
}

AVPacket* pktqueue_read_request_a(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    AVPacket *pkt = NULL;
    if (0 != sem_trywait(&ppq->asem)) return NULL;
    pkt = ppq->apkts[ppq->ahead++];
    if (ppq->ahead == ppq->asize) ppq->ahead = 0;
    return pkt;
}

void pktqueue_read_done_a(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail++] = pkt;
    if (ppq->ftail == ppq->fsize) ppq->ftail = 0;
    sem_post(&ppq->fsem);
}

AVPacket* pktqueue_read_request_v(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    AVPacket *pkt = NULL;
    if (0 != sem_trywait(&ppq->vsem)) return NULL;
    pkt = ppq->vpkts[ppq->vhead++];
    if (ppq->vhead == ppq->vsize) ppq->vhead = 0;
    return pkt;
}

void pktqueue_read_done_v(void *ctxt, AVPacket *pkt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail++] = pkt;
    if (ppq->ftail == ppq->fsize) ppq->ftail = 0;
    sem_post(&ppq->fsem);
}



