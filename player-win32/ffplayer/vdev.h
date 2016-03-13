#ifndef _NES_VDEV_H_
#define _NES_VDEV_H_

// 包含头文件
#include "corerender.h"

#ifdef __cplusplus
extern "C" {
#endif

// 函数声明
void* vdev_gdi_create  (void *surface, int bufnum, int w, int h, int frate);
void  vdev_gdi_destroy (void *ctxt);
void  vdev_gdi_request (void *ctxt, void **buf, int *stride);
void  vdev_gdi_post    (void *ctxt, int64_t pts);
void  vdev_gdi_setrect (void *ctxt, int x, int y, int w, int h);
void  vdev_gdi_pause   (void *ctxt, BOOL pause);
void  vdev_gdi_reset   (void *ctxt);
void  vdev_gdi_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts);

#define vdev_create     vdev_gdi_create
#define vdev_destroy    vdev_gdi_destroy
#define vdev_request    vdev_gdi_request
#define vdev_post       vdev_gdi_post
#define vdev_setrect    vdev_gdi_setrect
#define vdev_pause      vdev_gdi_pause
#define vdev_reset      vdev_gdi_reset
#define vdev_getavpts   vdev_gdi_getavpts

#ifdef __cplusplus
}
#endif

#endif



