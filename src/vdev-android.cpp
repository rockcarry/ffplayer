// 包含头文件
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 内部常量定义
#define DEF_WIN_PIX_FMT         HAL_PIXEL_FORMAT_YCrCb_420_SP // HAL_PIXEL_FORMAT_RGBX_8888 or HAL_PIXEL_FORMAT_YCrCb_420_SP
#define CAMDEV_GRALLOC_USAGE    GRALLOC_USAGE_SW_READ_NEVER \
                                    | GRALLOC_USAGE_SW_WRITE_NEVER \
                                    | GRALLOC_USAGE_HW_TEXTURE

// 内部类型定义
typedef struct
{
    // common members
    VDEV_COMMON_MEMBERS
} VDEVGDICTXT;

// 接口函数实现
void* vdev_android_create(void *surface, int bufnum, int w, int h, int frate)
{    return NULL;
}

void vdev_android_destroy(void *ctxt)
{
}

void vdev_android_request(void *ctxt, void **buffer, int *stride)
{
}

void vdev_android_post(void *ctxt, int64_t pts)
{
}

void vdev_android_setrect(void *ctxt, int x, int y, int w, int h)
{
}


