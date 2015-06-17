#define LOG_TAG "ffplayer_jni"

#include <jni.h>
#include <pthread.h>
#include <utils/Log.h>
#include "pktqueue.h"
#include "corerender.h"
#include "com_rockcarry_ffplayer_player.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

// namespace android
using namespace android;

// 内部常量定义
#define RENDER_LETTERBOX        0
#define RENDER_STRETCHED        1

#define PARAM_VIDEO_WIDTH       0
#define PARAM_VIDEO_HEIGHT      1
#define PARAM_VIDEO_DURATION    2
#define PARAM_VIDEO_POSITION    3
#define PARAM_RENDER_MODE       4

#define avcodec_decode_video avcodec_decode_video2
#define avcodec_decode_audio avcodec_decode_audio4

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
            usleep(20*1000);
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
            av_free_packet(packet); // free packet
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
            usleep(20*1000);
            continue;
        }
        //-- when audio decoding pause --//

        // read packet
        pktqueue_read_request_a(&(player->PacketQueue), &packet);

        //++ play completed ++//
        if (packet->pts == -1) {
            renderaudiowrite(player->hCoreRender, (AVFrame*)-1);
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
                    ALOGE("an error occurred during decoding audio.\n");
                    break;
                }

                if (gotaudio) {
                    aframe->pts = (int64_t)(aframe->pkt_pts * player->dAudioTimeBase);
                    renderaudiowrite(player->hCoreRender, aframe);
                }

                packet->data += consumed;
                packet->size -= consumed;
            }
        }
        //-- decode audio packet --//

        // free packet
        av_free_packet(packet);

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
            usleep(20*1000);
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
                    ALOGE("an error occurred during decoding video.\n");
                    break;
                }

                if (gotvideo) {
//                  vframe->pts = av_frame_get_best_effort_timestamp(vframe);
                    vframe->pts = (int64_t)(vframe->pkt_pts * player->dVideoTimeBase);
                    rendervideowrite(player->hCoreRender, vframe);
                }

                packet->data += consumed;
                packet->size -= consumed;
            }
        }
        //-- decode video packet --//

        // free packet
        av_free_packet(packet);

        pktqueue_read_done_v(&(player->PacketQueue));
    }

    av_frame_free(&vframe);
    return NULL;
}


/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeOpen
 * Signature: (Ljava/lang/String;Ljava/lang/Object;II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeOpen
  (JNIEnv *env, jclass cls, jstring url, jobject surface, jint w, jint h)
{
    ALOGD("Java_com_rockcarry_ffplayer_player_open.");
#if 1
    sp<Surface> surf = android_view_Surface_getSurface(env, surface);
    if (android::Surface::isValid(surf)) {
		ALOGE("surface is valid .");
	} else {
		ALOGE("surface is invalid.");
	}
#endif

    PLAYER        *player   = new PLAYER();
    AVCodec       *pAVCodec = NULL;
    int            vformat  = 0;
    int            width    = 0;
    int            height   = 0;
    AVRational     vrate    = {1, 1};
    uint64_t       alayout  = 0;
    int            aformat  = 0;
    int            arate    = 0;
    uint32_t       i        = 0;

    // av register all
    av_register_all();

    // clear player context
    memset(player, 0, sizeof(PLAYER));

    // create packet queue
    pktqueue_create(&(player->PacketQueue));

    // open input file
    if (avformat_open_input(&(player->pAVFormatContext), env->GetStringUTFChars(url, NULL), NULL, 0) != 0) {
        ALOGE("failed to open input url: %s !", env->GetStringUTFChars(url, NULL));
        goto error_handler;
    }

    // find stream info
    if (avformat_find_stream_info(player->pAVFormatContext, NULL) < 0) {
        ALOGE("failed to find stream info !");
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
            vrate = player->pAVFormatContext->streams[i]->r_frame_rate;
            break;

        default: break;
        }
    }

    // open audio codec
    if (player->iAudioStreamIndex != -1)
    {
        pAVCodec = avcodec_find_decoder(player->pAudioCodecContext->codec_id);
        if (pAVCodec)
        {
            if (avcodec_open2(player->pAudioCodecContext, pAVCodec, NULL) < 0)
            {
                player->iAudioStreamIndex = -1;
            }
        }
        else player->iAudioStreamIndex = -1;
    }

    // open video codec
    if (player->iVideoStreamIndex != -1)
    {
        pAVCodec = avcodec_find_decoder(player->pVideoCodecContext->codec_id);
        if (pAVCodec)
        {
            if (avcodec_open2(player->pVideoCodecContext, pAVCodec, NULL) < 0)
            {
                player->iVideoStreamIndex = -1;
            }
        }
        else player->iVideoStreamIndex = -1;
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
        aformat = player->pAudioCodecContext->sample_fmt;
        arate   = player->pAudioCodecContext->sample_rate;
    }

    // open core render
    player->hCoreRender = renderopen(surf, w, h,
        vrate, vformat, width, height,
        arate, aformat, alayout);

    // make sure player status paused
    player->nPlayerStatus = 0xf;
    pthread_create(&(player->hAVDemuxThread), NULL, AVDemuxThreadProc    , player);
    pthread_create(&(player->hADecodeThread), NULL, AudioDecodeThreadProc, player);
    pthread_create(&(player->hVDecodeThread), NULL, VideoDecodeThreadProc, player);

    return (jint)player;

error_handler:
    Java_com_rockcarry_ffplayer_player_nativeClose(env, cls, (jint)player);
    return 0;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeClose
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeClose
  (JNIEnv *env, jclass cls, jint hplayer)
{
    ALOGD("Java_com_rockcarry_ffplayer_player_nativeClose.");

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

    delete player;
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePlay
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePlay
  (JNIEnv *env, jclass cls, jint hplayer)
{
    ALOGD("Java_com_rockcarry_ffplayer_player_nativePlay.");
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;
    player->nPlayerStatus = 0;
    renderstart(player->hCoreRender);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativePause
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativePause
  (JNIEnv *env, jclass cls, jint hplayer)
{
    ALOGD("Java_com_rockcarry_ffplayer_player_nativePause.");
    PLAYER *player = (PLAYER*)hplayer;
    player->nPlayerStatus |= PS_R_PAUSE;
    renderpause(player->hCoreRender);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSeek
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSeek
  (JNIEnv *env, jclass cls, jint hplayer, jint sec )
{
    ALOGD("Java_com_rockcarry_ffplayer_player_nativeSeek.");
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    // start render if paused
    if (player->nPlayerStatus & PS_R_PAUSE) renderstart(player->hCoreRender);

    // render seek start
    player->nPlayerStatus |= PS_D_PAUSE;
    renderseek(player->hCoreRender, sec);

    // wait for packet queue empty
    while (!pktqueue_isempty_a(&(player->PacketQueue))) usleep(20*1000);
    while (!pktqueue_isempty_v(&(player->PacketQueue))) usleep(20*1000);

    // seek frame
    av_seek_frame(player->pAVFormatContext, -1, (int64_t)sec * AV_TIME_BASE, 0);
    if (player->iAudioStreamIndex != -1) avcodec_flush_buffers(player->pAudioCodecContext);
    if (player->iVideoStreamIndex != -1) avcodec_flush_buffers(player->pVideoCodecContext);

    // render seek done, -1 means done
    renderseek(player->hCoreRender, -1);
    player->nPlayerStatus &= ~PS_D_PAUSE;

    // wait for video packet queue not empty witch timeout 200ms
    int i = 10; while (i-- && pktqueue_isempty_v(&(player->PacketQueue))) usleep(20*1000);

    // pause render if needed
    if (player->nPlayerStatus & PS_R_PAUSE) renderpause(player->hCoreRender);
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeSetParam
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_com_rockcarry_ffplayer_player_nativeSetParam
  (JNIEnv *env, jclass cls, jint hplayer, jint id, jint value)
{
    ALOGD("Java_com_rockcarry_ffplayer_player_nativeSetParam.");
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_RENDER_MODE:
        player->nRenderMode = value;
        break;

    default: break;
    }
}

/*
 * Class:     com_rockcarry_ffplayer_player
 * Method:    nativeGetParam
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_rockcarry_ffplayer_player_nativeGetParam
  (JNIEnv *env, jclass cls, jint hplayer, jint id)
{
    ALOGD("Java_com_rockcarry_ffplayer_player_nativeGetParam.");

    if (!hplayer) return 0;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_VIDEO_WIDTH:
        if (!player->pVideoCodecContext) return 0;
        else return player->pVideoCodecContext->width;

    case PARAM_VIDEO_HEIGHT:
        if (!player->pVideoCodecContext) return 0;
        else return player->pVideoCodecContext->height;

    case PARAM_VIDEO_DURATION:
        if (!player->pAVFormatContext) return 0;
        else return (jint)(player->pAVFormatContext->duration / AV_TIME_BASE);
        break;

    case PARAM_VIDEO_POSITION:
        return rendertime(player->hCoreRender);

    case PARAM_RENDER_MODE:
        return player->nRenderMode;

    default: break;
    }
    
    return 0;
}

