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
    RENDER_LETTERBOX,
    RENDER_STRETCHED,
};

// param
enum {
    PARAM_VIDEO_WIDTH,
    PARAM_VIDEO_HEIGHT,
    PARAM_VIDEO_DURATION,
    PARAM_VIDEO_POSITION,
    PARAM_RENDER_MODE,
};

// 函数声明
void* playeropen    (char *file, void *extra);
void  playerclose   (void *hplayer);
void  playerplay    (void *hplayer);
void  playerpause   (void *hplayer);
void  playerseek    (void *hplayer, DWORD sec);
void  playersetrect (void *hplayer, int x, int y, int w, int h);
void  playersetparam(void *hplayer, DWORD id, DWORD param);
void  playergetparam(void *hplayer, DWORD id, void *param);

#ifdef __cplusplus
}
#endif

#endif















