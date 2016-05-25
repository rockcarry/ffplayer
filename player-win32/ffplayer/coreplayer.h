#ifndef __FFPLAYER_COREPLAYER_H__
#define __FFPLAYER_COREPLAYER_H__

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
// message
#define MSG_COREPLAYER  (WM_APP + 1)
#define PLAY_PROGRESS   (('R' << 24) | ('U' << 16) | ('N' << 8))
#define PLAY_COMPLETED  (('E' << 24) | ('N' << 16) | ('D' << 8))

// render mode
enum {
    VIDEO_MODE_LETTERBOX,
    VIDEO_MODE_STRETCHED,
    VIDEO_MODE_MAX_NUM,
};

enum {
    VISUAL_EFFECT_DISABLE ,
    VISUAL_EFFECT_WAVEFORM,
    VISUAL_EFFECT_SPECTRUM,
    VISUAL_EFFECT_MAX_NUM,
};

// param
enum {
    //++ public
    // duration & position
    PARAM_MEDIA_DURATION = 0x1000,
    PARAM_MEDIA_POSITION,

    // media detail info
    PARAM_VIDEO_WIDTH,
    PARAM_VIDEO_HEIGHT,

    // video display mode
    PARAM_VIDEO_MODE,

    // audio volume control
    PARAM_AUDIO_VOLUME,

    // playback speed control
    PARAM_PLAY_SPEED,
    PARAM_AUTO_SLOW_DOWN,
    PARAM_MIN_PLAY_SPEED,
    PARAM_MAX_PLAY_SPEED,

    // visual effect mode
    PARAM_VISUAL_EFFECT,

    // player event callback
    PARAM_PLAYER_CALLBACK,
    //-- public

    //++ for vdev
    PARAM_VDEV_PIXEL_FORMAT = 0x2000,
    PARAM_VDEV_FRAME_RATE,
    PARAM_VDEV_SLOW_FLAG,
    PARAM_VDEV_SURFACE_WIDTH,
    PARAM_VDEV_SURFACE_HEIGHT,
    //-- for vdev

    //++ for adev
    PARAM_ADEV_SLOW_FLAG = 0x3000,
    //-- for adev
};

// player event callback
typedef void (*PFN_PLAYER_CALLBACK)(__int32 msg, __int64 param);

// 函数声明
void* player_open    (char *file, void *extra);
void  player_close   (void *hplayer);
void  player_play    (void *hplayer);
void  player_pause   (void *hplayer);
void  player_seek    (void *hplayer, LONGLONG ms);
void  player_setrect (void *hplayer, int type, int x, int y, int w, int h); // type: 0 - video rect, 1 - visual effect rect
void  player_setparam(void *hplayer, DWORD id, void *param);
void  player_getparam(void *hplayer, DWORD id, void *param);

#ifdef __cplusplus
}
#endif

#endif




