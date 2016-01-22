// 包含头文件
#include <pthread.h>
#include "pktqueue.h"
#include "coreplayer.h"
#include "corerender.h"
#include "log.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

// 内部常量定义
#define avcodec_decode_video avcodec_decode_video2
#define avcodec_decode_audio avcodec_decode_audio4
#define avcodec_open         avcodec_open2

// 内部类型定义
typedef struct
{
    // audio
    AVFormatContext *pAVFormatContext;
    AVCodecContext  *pAudioCodecContext;
    int              iAudioStreamIndex;
    double           dAudioTimeBase;

    // video
    AVCodecContext  *pVideoCodecContext;
    int              iVideoStreamIndex;
    double           dVideoTimeBase;
    AVRational       tVideoFrameRate;

    // render
    int              nRenderMode;
    void            *hCoreRender;

    // thread
    #define PS_D_PAUSE  (1 << 0)  // demux pause
    #define PS_A_PAUSE  (1 << 1)  // audio decoding pause
    #define PS_V_PAUSE  (1 << 2)  // video decoding pause
    #define PS_R_PAUSE  (1 << 3)  // rendering pause
    #define PS_CLOSE    (1 << 4)  // close player
    int              nPlayerStatus;
    pthread_t        hAVDemuxThread;
    pthread_t        hADecodeThread;
    pthread_t        hVDecodeThread;

    // packet queue
    PKTQUEUE         PacketQueue;
} PLAYER;

// 内部函数实现
static void* AVDemuxThreadProc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    int       retv   = 0;

    while (!(player->nPlayerStatus & PS_CLOSE))
    {
        //++ when demux pause ++//
        if (player->nPlayerStatus & PS_D_PAUSE) {
            Sleep(20);
            continue;
        }
        //-- when demux pause --//

        pktqueue_write_request(&(player->PacketQueue), &packet);
        retv = av_read_frame(player->pAVFormatContext, packet);

        //++ play completed ++//
        if (retv < 0)
        {
            packet->pts = -1; // video packet pts == -1, means completed
            pktqueue_write_done_a(&(player->PacketQueue));
            player->nPlayerStatus |= PS_D_PAUSE;
            continue;
        }
        //-- play completed --//

        // audio
        if (packet->stream_index == player->iAudioStreamIndex)
        {
            pktqueue_write_done_a(&(player->PacketQueue));
        }

        // video
        if (packet->stream_index == player->iVideoStreamIndex)
        {
            pktqueue_write_done_v(&(player->PacketQueue));
        }

        if (  packet->stream_index != player->iAudioStreamIndex
           && packet->stream_index != player->iVideoStreamIndex )
        {
            av_packet_unref(packet); // free packet
            pktqueue_write_release(&(player->PacketQueue));
        }
    }

    return NULL;
}

static void* AudioDecodeThreadProc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    AVFrame  *aframe = NULL;

    aframe = av_frame_alloc();
    if (!aframe) return NULL;

    while (!(player->nPlayerStatus & PS_CLOSE))
    {
        //++ when audio decoding pause ++//
        if (player->nPlayerStatus & PS_A_PAUSE) {
            Sleep(20);
            continue;
        }
        //-- when audio decoding pause --//

        // read packet
        pktqueue_read_request_a(&(player->PacketQueue), &packet);

        //++ play completed ++//
        if (packet->pts == -1) {
            renderwriteaudio(player->hCoreRender, (AVFrame*)-1);
            pktqueue_read_done_a(&(player->PacketQueue));
            continue;
        }
        //-- play completed --//

        //++ decode audio packet ++//
        if (player->iAudioStreamIndex != -1) {
            while (packet->size > 0) {
                int consumed = 0;
                int gotaudio = 0;
                consumed = avcodec_decode_audio(player->pAudioCodecContext, aframe, &gotaudio, packet);
                if (consumed < 0) {
                    log_printf(TEXT("an error occurred during decoding audio.\n"));
                    break;
                }

                if (gotaudio) {
                    aframe->pts = (int64_t)(av_frame_get_best_effort_timestamp(aframe) * player->dAudioTimeBase);
                    renderwriteaudio(player->hCoreRender, aframe);
                }

                packet->data += consumed;
                packet->size -= consumed;
            }
        }
        //-- decode audio packet --//

        // free packet
        av_packet_unref(packet);

        pktqueue_read_done_a(&(player->PacketQueue));
    }

    av_frame_free(&aframe);
    return NULL;
}

static void* VideoDecodeThreadProc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    AVFrame  *vframe = NULL;

    vframe = av_frame_alloc();
    if (!vframe) return NULL;

    while (!(player->nPlayerStatus & PS_CLOSE))
    {
        //++ when video decoding pause ++//
        if (player->nPlayerStatus & PS_V_PAUSE) {
            Sleep(20);
            continue;
        }
        //-- when video decoding pause --//

        // read packet
        pktqueue_read_request_v(&(player->PacketQueue), &packet);

        //++ decode video packet ++//
        if (player->iVideoStreamIndex != -1) {
            while (packet->size > 0) {
                int consumed = 0;
                int gotvideo = 0;

                consumed = avcodec_decode_video(player->pVideoCodecContext, vframe, &gotvideo, packet);
                if (consumed < 0) {
                    log_printf(TEXT("an error occurred during decoding video.\n"));
                    break;
                }

                if (gotvideo) {
                    vframe->pts = (int64_t)(av_frame_get_best_effort_timestamp(vframe) * player->dVideoTimeBase);
                    renderwritevideo(player->hCoreRender, vframe);
                }

                packet->data += consumed;
                packet->size -= consumed;
            }
        }
        //-- decode video packet --//

        // free packet
        av_packet_unref(packet);

        pktqueue_read_done_v(&(player->PacketQueue));
    }

    av_frame_free(&vframe);
    return NULL;
}

// 函数实现
void* playeropen(char *file, void *extra)
{
    PLAYER        *player   = NULL;
    AVCodec       *decoder = NULL;
    int            vformat  = 0;
    int            width    = 0;
    int            height   = 0;
    uint64_t       alayout  = 0;
    int            aformat  = 0;
    int            arate    = 0;
    uint32_t       i        = 0;

    // init log
//  log_init(TEXT("DEBUGER"));

    // av register all
    av_register_all();

    // alloc player context
    player = (PLAYER*)malloc(sizeof(PLAYER));
    memset(player, 0, sizeof(PLAYER));

    // create packet queue
    pktqueue_create(&(player->PacketQueue));

    // open input file
    if (avformat_open_input(&(player->pAVFormatContext), file, NULL, 0) != 0) {
        goto error_handler;
    }

    // find stream info
    if (avformat_find_stream_info(player->pAVFormatContext, NULL) < 0) {
        goto error_handler;
    }

    // get video & audio codec context
    player->iAudioStreamIndex = -1;
    player->iVideoStreamIndex = -1;
    for (i=0; i<player->pAVFormatContext->nb_streams; i++)
    {
        switch (player->pAVFormatContext->streams[i]->codec->codec_type)
        {
        case AVMEDIA_TYPE_AUDIO:
            player->iAudioStreamIndex  = i;
            player->pAudioCodecContext = player->pAVFormatContext->streams[i]->codec;
            player->dAudioTimeBase     = av_q2d(player->pAVFormatContext->streams[i]->time_base) * 1000;
            break;

        case AVMEDIA_TYPE_VIDEO:
            player->iVideoStreamIndex  = i;
            player->pVideoCodecContext = player->pAVFormatContext->streams[i]->codec;
            player->dVideoTimeBase     = av_q2d(player->pAVFormatContext->streams[i]->time_base) * 1000;
            player->tVideoFrameRate    = player->pAVFormatContext->streams[i]->r_frame_rate;
            if (player->tVideoFrameRate.num / player->tVideoFrameRate.den > 100) {
                player->tVideoFrameRate.num = 30;
                player->tVideoFrameRate.den = 1;
            }
            break;
        }
    }

    // open audio codec
    if (player->iAudioStreamIndex != -1)
    {
        decoder = avcodec_find_decoder(player->pAudioCodecContext->codec_id);
        if (!decoder || avcodec_open(player->pAudioCodecContext, decoder, NULL) < 0)
        {
            log_printf(TEXT("failed to find or open decoder for audio !\n"));
            player->iAudioStreamIndex = -1;
        }
    }

    // open video codec
    if (player->iVideoStreamIndex != -1)
    {
        decoder = avcodec_find_decoder(player->pVideoCodecContext->codec_id);
        if (!decoder || avcodec_open(player->pVideoCodecContext, decoder, NULL) < 0)
        {
            log_printf(TEXT("failed to find or open decoder for video !\n"));
            player->iVideoStreamIndex = -1;
        }
    }

    // for video
    if (player->iVideoStreamIndex != -1)
    {
        vformat = player->pVideoCodecContext->pix_fmt;
        width   = player->pVideoCodecContext->width;
        height  = player->pVideoCodecContext->height;
    }

    // for audio
    if (player->iAudioStreamIndex != -1)
    {
        alayout = player->pAudioCodecContext->channel_layout;
        //++ fix audio channel layout issue
        if (alayout == 0) {
            alayout = av_get_default_channel_layout(player->pAudioCodecContext->channels);
        }
        //-- fix audio channel layout issue
        aformat = player->pAudioCodecContext->sample_fmt;
        arate   = player->pAudioCodecContext->sample_rate;
    }

    // open core render
    player->hCoreRender = renderopen(extra, player->tVideoFrameRate, vformat, width, height,
        arate, (AVSampleFormat)aformat, alayout);

    // make sure player status paused
    player->nPlayerStatus = 0xf;
    pthread_create(&(player->hAVDemuxThread), NULL, AVDemuxThreadProc    , player);
    pthread_create(&(player->hADecodeThread), NULL, AudioDecodeThreadProc, player);
    pthread_create(&(player->hVDecodeThread), NULL, VideoDecodeThreadProc, player);

    return player;

error_handler:
    playerclose(player);
    return NULL;
}

void playerclose(void *hplayer)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    player->nPlayerStatus = PS_CLOSE;
    renderstart(player->hCoreRender);

    //++ make sure packet queue not empty ++//
    if (pktqueue_isempty_a(&(player->PacketQueue))) {
        pktqueue_write_request(&(player->PacketQueue), NULL);
        pktqueue_write_done_a(&(player->PacketQueue));
    }

    if (pktqueue_isempty_v(&(player->PacketQueue))) {
        pktqueue_write_request(&(player->PacketQueue), NULL);
        pktqueue_write_done_v(&(player->PacketQueue));
    }
    //-- make sure packet queue not empty --//

    // wait audio/video demuxing thread exit
    pthread_join(player->hAVDemuxThread, NULL);

    // wait audio decoding thread exit
    pthread_join(player->hADecodeThread, NULL);

    // wait video decoding thread exit
    pthread_join(player->hVDecodeThread, NULL);

    // destroy packet queue
    pktqueue_destroy(&(player->PacketQueue));

    if (player->hCoreRender       ) renderclose(player->hCoreRender);
    if (player->pVideoCodecContext) avcodec_close(player->pVideoCodecContext);
    if (player->pAudioCodecContext) avcodec_close(player->pAudioCodecContext);
    if (player->pAVFormatContext  ) avformat_close_input(&(player->pAVFormatContext));

    free(player);

    // close log
//  log_done();
}

void playerplay(void *hplayer)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;
    player->nPlayerStatus = 0;
    renderstart(player->hCoreRender);
}

void playerpause(void *hplayer)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;
    player->nPlayerStatus |= PS_R_PAUSE;
    renderpause(player->hCoreRender);
}

void playersetrect(void *hplayer, int x, int y, int w, int h)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    int vw, vh;
    int rw, rh;
    playergetparam(hplayer, PARAM_VIDEO_WIDTH , &vw);
    playergetparam(hplayer, PARAM_VIDEO_HEIGHT, &vh);
    if (!vw || !vh) return;

    switch (player->nRenderMode)
    {
    case RENDER_LETTERBOX:
        if (w * vh < h * vw) { rw = w; rh = rw * vh / vw; }
        else                 { rh = h; rw = rh * vw / vh; }
        break;

    case RENDER_STRETCHED:
        rw = w;
        rh = h;
        break;
    }

    if (rw <= 0) rw = 1;
    if (rh <= 0) rh = 1;
    rendersetrect(player->hCoreRender, x + (w - rw) / 2, y + (h - rh) / 2, rw, rh);
}

void playerseek(void *hplayer, DWORD sec)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    // start render if paused
    if (player->nPlayerStatus & PS_R_PAUSE) renderstart(player->hCoreRender);

    // render seek start
    player->nPlayerStatus |= PS_D_PAUSE;
    renderseek(player->hCoreRender, sec);

    // wait for packet queue empty
    while (!pktqueue_isempty_a(&(player->PacketQueue))) Sleep(20);
    while (!pktqueue_isempty_v(&(player->PacketQueue))) Sleep(20);

    // seek frame
    av_seek_frame(player->pAVFormatContext, -1, (int64_t)sec * AV_TIME_BASE, 0);
    if (player->iAudioStreamIndex != -1) avcodec_flush_buffers(player->pAudioCodecContext);
    if (player->iVideoStreamIndex != -1) avcodec_flush_buffers(player->pVideoCodecContext);

    // render seek done, -1 means done
    renderseek(player->hCoreRender, -1);
    player->nPlayerStatus &= ~PS_D_PAUSE;

    // wait for video packet queue not empty witch timeout 200ms
    int i = 10; while (i-- && pktqueue_isempty_v(&(player->PacketQueue))) Sleep(20);

    // pause render if needed
    if (player->nPlayerStatus & PS_R_PAUSE) renderpause(player->hCoreRender);
}

void playersetparam(void *hplayer, DWORD id, DWORD param)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_RENDER_MODE:
        player->nRenderMode = param;
        break;
    }
}

void playergetparam(void *hplayer, DWORD id, void *param)
{
    if (!hplayer || !param) return;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_VIDEO_WIDTH:
        if (!player->pVideoCodecContext) *(int*)param = 0;
        else *(int*)param = player->pVideoCodecContext->width;
        break;

    case PARAM_VIDEO_HEIGHT:
        if (!player->pVideoCodecContext) *(int*)param = 0;
        else *(int*)param = player->pVideoCodecContext->height;
        break;

    case PARAM_VIDEO_DURATION:
        if (!player->pAVFormatContext) *(DWORD*)param = 0;
        else *(DWORD*)param = (DWORD)(player->pAVFormatContext->duration / AV_TIME_BASE);
        break;

    case PARAM_VIDEO_POSITION:
        rendertime(player->hCoreRender, (DWORD*)param);
        break;

    case PARAM_RENDER_MODE:
        *(int*)param = player->nRenderMode;
        break;
    }
}





