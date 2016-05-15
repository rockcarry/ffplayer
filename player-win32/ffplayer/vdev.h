#ifndef __FFPLAYER_VDEV_H__
#define __FFPLAYER_VDEV_H__

// 包含头文件
#include "corerender.h"

// 预编译开关
#define CLEAR_VDEV_WHEN_DESTROYED  TRUE
#define CLEAR_VDEV_WHEN_COMPLETED  TRUE

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
void  vdev_gdi_setparam(void *ctxt, DWORD id, void *param);
void  vdev_gdi_getparam(void *ctxt, DWORD id, void *param);

void* vdev_d3d_create  (void *surface, int bufnum, int w, int h, int frate);
void  vdev_d3d_destroy (void *ctxt);
void  vdev_d3d_request (void *ctxt, void **buf, int *stride);
void  vdev_d3d_post    (void *ctxt, int64_t pts);
void  vdev_d3d_setrect (void *ctxt, int x, int y, int w, int h);
void  vdev_d3d_pause   (void *ctxt, BOOL pause);
void  vdev_d3d_reset   (void *ctxt);
void  vdev_d3d_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts);
void  vdev_d3d_setparam(void *ctxt, DWORD id, void *param);
void  vdev_d3d_getparam(void *ctxt, DWORD id, void *param);

#if 1
#define vdev_create     vdev_gdi_create
#define vdev_destroy    vdev_gdi_destroy
#define vdev_request    vdev_gdi_request
#define vdev_post       vdev_gdi_post
#define vdev_setrect    vdev_gdi_setrect
#define vdev_pause      vdev_gdi_pause
#define vdev_reset      vdev_gdi_reset
#define vdev_getavpts   vdev_gdi_getavpts
#define vdev_getparam   vdev_gdi_getparam
#define vdev_setparam   vdev_gdi_setparam
#else
#define vdev_create     vdev_d3d_create
#define vdev_destroy    vdev_d3d_destroy
#define vdev_request    vdev_d3d_request
#define vdev_post       vdev_d3d_post
#define vdev_setrect    vdev_d3d_setrect
#define vdev_pause      vdev_d3d_pause
#define vdev_reset      vdev_d3d_reset
#define vdev_getavpts   vdev_d3d_getavpts
#define vdev_getparam   vdev_d3d_getparam
#define vdev_setparam   vdev_d3d_setparam
#endif

#ifdef __cplusplus
}
#endif

#endif



