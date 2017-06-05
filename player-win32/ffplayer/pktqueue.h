#ifndef __FFPLAYER_PKTQUEUE_H__
#define __FFPLAYER_PKTQUEUE_H__

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// avformat.h
#include "libavformat/avformat.h"

// 函数声明
void* pktqueue_create (int   size);
void  pktqueue_destroy(void *ctxt);
void  pktqueue_reset  (void *ctxt);

AVPacket* pktqueue_write_request (void *ctxt); // request a packet for write
AVPacket* pktqueue_read_request_a(void *ctxt); // request a packet for read audio
AVPacket* pktqueue_read_request_v(void *ctxt); // request a packet for read video

void  pktqueue_write_post_a(void *ctxt, AVPacket *pkt); // post a packet for write as audio
void  pktqueue_write_post_v(void *ctxt, AVPacket *pkt); // post a packet for write as video
void  pktqueue_write_post_i(void *ctxt, AVPacket *pkt); // post a packet for write as invalid
void  pktqueue_read_done_a (void *ctxt, AVPacket *pkt); // audio packet read done
void  pktqueue_read_done_v (void *ctxt, AVPacket *pkt); // video packet read done

#ifdef __cplusplus
}
#endif

#endif





