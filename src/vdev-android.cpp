// 包含头文件
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 内部类型定义
typedef struct
{
    int reserved;
} VDEVGDICTXT;

// 接口函数实现
void vdev_pause   (void *ctxt, int pause) {}
void vdev_reset   (void *ctxt) {}
void vdev_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts) {}
void vdev_setparam(void *ctxt, int id, void *param) {}
void vdev_getparam(void *ctxt, int id, void *param) {}
void vdev_player_event(void *ctxt, int32_t msg, int64_t param) {}
void vdev_refresh_background(void *ctxt) {}

void* vdev_create(int type, void *surface, int bufnum, int w, int h, int frate)
{    return NULL;
}

void vdev_destroy(void *ctxt)
{
}

void vdev_request(void *ctxt, void **buffer, int *stride)
{
}

void vdev_post(void *ctxt, int64_t pts)
{
}

void vdev_setrect(void *ctxt, int x, int y, int w, int h)
{
}


