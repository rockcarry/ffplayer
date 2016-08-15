#ifndef __FFPLAYER_VDEV_H__
#define __FFPLAYER_VDEV_H__

// 包含头文件
#include <stdint.h>
#include "ffplayer.h"

#ifdef __cplusplus
extern "C" {
#endif

// 预编译开关
#define CLEAR_VDEV_WHEN_DESTROYED  TRUE
#define CLEAR_VDEV_WHEN_COMPLETED  TRUE

// 常量定义
#define VDEV_CLOSE      (1 << 0)
#define VDEV_PAUSE      (1 << 1)
#define VDEV_COMPLETED  (1 << 2)

//++ vdev context common members
#define VDEV_COMMON_MEMBERS \
    int      type;   \
    int      bufnum; \
    int      pixfmt; \
    int      x;   /* video display rect x */ \
    int      y;   /* video display rect y */ \
    int      w;   /* video display rect w */ \
    int      h;   /* video display rect h */ \
    int      sw;  /* surface width        */ \
    int      sh;  /* surface height       */ \
                                             \
    HWND     hwnd;                           \
    int64_t *ppts;                           \
    int64_t  apts;                           \
    int64_t  vpts;                           \
                                             \
    int      head;                           \
    int      tail;                           \
    HANDLE   semr;                           \
    HANDLE   semw;                           \
                                             \
    int      tickavdiff;                     \
    int      tickframe;                      \
    int      ticksleep;                      \
    int      ticklast;                       \
                                             \
    int      status;                         \
    HANDLE   thread;                         \
                                             \
    int      completed_counter;              \
    int64_t  completed_apts;                 \
    int64_t  completed_vpts;                 \
    int      refresh_flag;                   \
                                             \
    PFN_PLAYER_CALLBACK fpcb;
//-- vdev context common members

// 类型定义
typedef struct {
    VDEV_COMMON_MEMBERS
} VDEV_COMMON_CTXT;

// 函数声明
void  vdev_setrect (void *ctxt, int x, int y, int w, int h);
void  vdev_pause   (void *ctxt, BOOL pause);
void  vdev_reset   (void *ctxt);
void  vdev_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts);
void  vdev_setparam(void *ctxt, DWORD id, void *param);
void  vdev_getparam(void *ctxt, DWORD id, void *param);
void  vdev_player_event(void *ctxt, int32_t msg, int64_t param);
void  vdev_refresh_background(void *ctxt);

void* vdev_create  (int type, void *surface, int bufnum, int w, int h, int frate);
void  vdev_destroy (void *ctxt);
void  vdev_request (void *ctxt, void **buf, int *stride);
void  vdev_post    (void *ctxt, int64_t pts);
void  vdev_setrect (void *ctxt, int x, int y, int w, int h);

// vdev-gdi
void* vdev_gdi_create (void *surface, int bufnum, int w, int h, int frate);
void  vdev_gdi_destroy(void *ctxt);
void  vdev_gdi_request(void *ctxt, void **buf, int *stride);
void  vdev_gdi_post   (void *ctxt, int64_t pts);
void  vdev_gdi_setrect(void *ctxt, int x, int y, int w, int h);

// vdev-d3d
void* vdev_d3d_create (void *surface, int bufnum, int w, int h, int frate);
void  vdev_d3d_destroy(void *ctxt);
void  vdev_d3d_request(void *ctxt, void **buf, int *stride);
void  vdev_d3d_post   (void *ctxt, int64_t pts);
void  vdev_d3d_setrect(void *ctxt, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif

#endif



