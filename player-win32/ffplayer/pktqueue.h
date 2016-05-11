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

BOOL  pktqueue_write_request (void *ctxt, AVPacket **pppkt);
void  pktqueue_write_cancel  (void *ctxt);
void  pktqueue_write_post_a  (void *ctxt);
void  pktqueue_write_post_v  (void *ctxt);

BOOL  pktqueue_read_request_a(void *ctxt, AVPacket **pppkt);
void  pktqueue_read_post_a   (void *ctxt);

BOOL  pktqueue_read_request_v(void *ctxt, AVPacket **pppkt);
void  pktqueue_read_post_v   (void *ctxt);

#ifdef __cplusplus
}
#endif

#endif





