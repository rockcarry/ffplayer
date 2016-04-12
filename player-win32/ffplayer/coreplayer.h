#ifndef __CORE_PLAYER_H__
#define __CORE_PLAYER_H__

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
// message
#define MSG_COREPLAYER  (WM_APP + 1)
#define PLAY_COMPLETED  (('E' << 24) | ('N' << 16) | ('D' << 8))

// render mode
enum {
    VIDEO_MODE_LETTERBOX,
    VIDEO_MODE_STRETCHED,
};

// param
enum {
    PARAM_VIDEO_WIDTH,
    PARAM_VIDEO_HEIGHT,
    PARAM_VIDEO_DURATION,
    PARAM_VIDEO_POSITION,
    PARAM_VIDEO_MODE,
    PARAM_AUDIO_VOLUME,
    PARAM_PLAYER_SPEED,
    PARAM_AUTO_SLOW_DOWN,
    PARAM_MIN_PLAY_SPEED,
    PARAM_MAX_PLAY_SPEED,
};

// 函数声明
void* player_open    (char *file, void *extra);
void  player_close   (void *hplayer);
void  player_play    (void *hplayer);
void  player_pause   (void *hplayer);
void  player_seek    (void *hplayer, LONGLONG ms);
void  player_setrect (void *hplayer, int x, int y, int w, int h);
void  player_setparam(void *hplayer, DWORD id, void *param);
void  player_getparam(void *hplayer, DWORD id, void *param);

#ifdef __cplusplus
}
#endif

#endif















