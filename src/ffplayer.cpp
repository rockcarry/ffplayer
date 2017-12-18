// 包含头文件
#include <pthread.h>
#include "pktqueue.h"
#include "ffrender.h"
#include "ffplayer.h"

extern "C" {
#include "libavutil/time.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

// 内部类型定义
typedef struct
{
    // format
    AVFormatContext *avformat_context;

    // audio
    AVCodecContext  *acodec_context;
    int              astream_index;
    AVRational       astream_timebase;

    // video
    AVCodecContext  *vcodec_context;
    int              vstream_index;
    AVRational       vstream_timebase;

    // pktqueue
    void            *pktqueue;

    // render
    void            *render;
    RECT             vdrect;
    int              vdmode;

    // thread
    #define PS_A_PAUSE    (1 << 0)  // audio decoding pause
    #define PS_V_PAUSE    (1 << 1)  // video decoding pause
    #define PS_R_PAUSE    (1 << 2)  // rendering pause
    #define PS_F_SEEK     (1 << 3)  // seek flag
    #define PS_A_SEEK     (1 << 4)  // seek audio
    #define PS_V_SEEK     (1 << 5)  // seek video
    #define PS_CLOSE      (1 << 6)  // close player
    int              player_status; // bits[18:16] used for seek type
    int64_t          seek_dest;
    int64_t          seek_type;
    int64_t          start_pts;

    pthread_t        avdemux_thread;
    pthread_t        adecode_thread;
    pthread_t        vdecode_thread;

    AVFilterGraph   *vfilter_graph;
    AVFilterContext *vfilter_source_ctx;
    AVFilterContext *vfilter_yadif_ctx;
    AVFilterContext *vfilter_sink_ctx;
    int              vfilter_enable;

    // for player init timeout
    int64_t          init_timetick;
    int64_t          init_timeout;

    PLAYER_INIT_PARAMS init_params;
} PLAYER;

// 内部常量定义
static const AVRational TIMEBASE_MS = { 1, 1000 };

// 内部函数实现
static void ffplayer_log_callback(void* ptr, int level, const char *fmt, va_list vl) {
    DO_USE_VAR(ptr);
    if (level <= av_log_get_level()) {
        char str[1024];
#ifdef WIN32
        vsprintf_s(str, 1024, fmt, vl);
        OutputDebugStringA(str);
#endif
#ifdef ANDROID
        vsprintf(str, fmt, vl);
        ALOGD("%s", str);
#endif
    }
}

//++ for filter graph
static void vfilter_graph_init(PLAYER *player)
{
    AVFilter *filter_source  = avfilter_get_by_name("buffer");
    AVFilter *filter_yadif   = avfilter_get_by_name("yadif" );
    AVFilter *filter_sink    = avfilter_get_by_name("buffersink");
    AVCodecContext *vdec_ctx = player->vcodec_context;
    char      args[256];
    int       ret;

    if (!player->vcodec_context) return;
    player->vfilter_graph = avfilter_graph_alloc();
    if (!player->vfilter_graph ) return;

    sprintf_s(args, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            vdec_ctx->width, vdec_ctx->height, vdec_ctx->pix_fmt,
            vdec_ctx->time_base.num, vdec_ctx->time_base.den,
            vdec_ctx->sample_aspect_ratio.num, vdec_ctx->sample_aspect_ratio.den);
    avfilter_graph_create_filter(&player->vfilter_source_ctx, filter_source, "in"   , args, NULL, player->vfilter_graph);
    avfilter_graph_create_filter(&player->vfilter_yadif_ctx , filter_yadif , "yadif", "mode=send_frame:parity=auto:deint=interlaced", NULL, player->vfilter_graph);
    avfilter_graph_create_filter(&player->vfilter_sink_ctx  , filter_sink  , "out"  , NULL, NULL, player->vfilter_graph);
    avfilter_link(player->vfilter_source_ctx, 0, player->vfilter_yadif_ctx, 0);
    avfilter_link(player->vfilter_yadif_ctx , 0, player->vfilter_sink_ctx , 0);

    ret = avfilter_graph_config(player->vfilter_graph, NULL);
    if (ret < 0) {
        avfilter_graph_free(&player->vfilter_graph);
        player->vfilter_graph = NULL;
    }
}

static void vfilter_graph_free(PLAYER *player)
{
    if (!player->vfilter_graph ) return;
    avfilter_graph_free(&player->vfilter_graph);
    player->vfilter_graph = NULL;
}

static void vfilter_graph_input(PLAYER *player, AVFrame *frame)
{
    if (!player->vfilter_graph ) return;
    if (!player->vfilter_enable) return;
    av_buffersrc_add_frame_flags(player->vfilter_source_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
}

static int vfilter_graph_output(PLAYER *player, AVFrame *frame)
{
    if (player->vfilter_graph && player->vfilter_enable) {
        return av_buffersink_get_frame(player->vfilter_sink_ctx, frame);
    } else {
        return 0;
    }
}
//-- for filter graph

static void player_handle_fseek_flag(PLAYER *player)
{
    int PAUSE_REQ = 0;
    int PAUSE_ACK = 0;
    if (player->astream_index != -1) { PAUSE_REQ |= PS_A_PAUSE; PAUSE_ACK |= PS_A_PAUSE << 16; }
    if (player->vstream_index != -1) { PAUSE_REQ |= PS_V_PAUSE; PAUSE_ACK |= PS_V_PAUSE << 16; }
    // make audio & video decoding thread pause
    player->player_status |= PAUSE_REQ;
    player->player_status &=~PAUSE_ACK;
    // wait for pause done
    while ((player->player_status & PAUSE_ACK) != PAUSE_ACK) usleep(20*1000);

    // seek frame
    av_seek_frame(player->avformat_context, -1, player->seek_dest / 1000 * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
    if (player->astream_index != -1) avcodec_flush_buffers(player->acodec_context);
    if (player->vstream_index != -1) avcodec_flush_buffers(player->vcodec_context);

    pktqueue_reset(player->pktqueue); // reset pktqueue
    render_reset  (player->render  ); // reset render

    //++ seek to dest pts
    if (player->seek_type) {
        int SEEK_REQ = 0;
        if (player->astream_index != -1) SEEK_REQ |= PS_A_SEEK;
        if (player->vstream_index != -1) SEEK_REQ |= PS_V_SEEK;
        player->player_status |= SEEK_REQ;
    }
    //-- seek to dest pts

    // make audio & video decoding thread resume
    player->player_status &= ~(PAUSE_REQ|PAUSE_ACK);
}

static void* av_demux_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    int       retv   = 0;

    while (!(player->player_status & PS_CLOSE))
    {
        //++ when demux pause ++//
        if (player->player_status & PS_F_SEEK) {
            player->player_status &= ~PS_F_SEEK;
            player_handle_fseek_flag(player);
        }
        //-- when demux pause --//

        packet = pktqueue_write_dequeue(player->pktqueue);
        if (packet == NULL) { usleep(20*1000); continue; }

        retv = av_read_frame(player->avformat_context, packet);
        if (retv < 0) {
            av_packet_unref(packet); // free packet
            pktqueue_write_cancel(player->pktqueue, packet);
            usleep(20*1000); continue;
        }

        // audio
        if (packet->stream_index == player->astream_index) {
            pktqueue_write_enqueue_a(player->pktqueue, packet);
        }

        // video
        if (packet->stream_index == player->vstream_index) {
            pktqueue_write_enqueue_v(player->pktqueue, packet);
        }

        if (  packet->stream_index != player->astream_index
           && packet->stream_index != player->vstream_index) {
            av_packet_unref(packet); // free packet
            pktqueue_write_cancel(player->pktqueue, packet);
        }
    }

    return NULL;
}

static void* audio_decode_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    AVFrame  *aframe = NULL;
    int64_t   apts;

    aframe = av_frame_alloc();
    if (!aframe) return NULL;
    else aframe->pts = -1;

    while (!(player->player_status & PS_CLOSE))
    {
        //++ when audio decode pause ++//
        if (player->player_status & PS_A_PAUSE) {
            player->player_status |= (PS_A_PAUSE << 16);
            usleep(20*1000); continue;
        }
        //-- when audio decode pause --//

        // read packet
        packet = pktqueue_read_dequeue_a(player->pktqueue);
        if (packet == NULL) {
//          render_audio(player->render, aframe);
            usleep(20*1000); continue;
        }

        //++ decode audio packet ++//
        apts = -1; // make it -1
        while (packet->size > 0 && !(player->player_status & (PS_A_PAUSE|PS_CLOSE))) {
            int consumed = 0;
            int gotaudio = 0;

            consumed = avcodec_decode_audio4(player->acodec_context, aframe, &gotaudio, packet);
            if (consumed < 0) {
                av_log(NULL, AV_LOG_WARNING, "an error occurred during decoding audio.\n");
                break;
            }

            if (gotaudio) {
                AVRational tb_sample_rate = { 1, player->acodec_context->sample_rate };
                if (apts == -1) {
                    apts  = av_rescale_q(aframe->pts, av_codec_get_pkt_timebase(player->acodec_context), tb_sample_rate);
                } else {
                    apts += aframe->nb_samples;
                }
                aframe->pts = av_rescale_q(apts, tb_sample_rate, TIMEBASE_MS);
                //++ for seek operation
                if (player->player_status & PS_A_SEEK) {
                    if (player->seek_dest - aframe->pts < 100) {
                        player->player_status &= ~PS_A_SEEK;
                    }
                    if ((player->player_status & PS_R_PAUSE) && player->vstream_index == -1) {
                        render_pause(player->render);
                    }
                }
                //-- for seek operation
                if (!(player->player_status & PS_A_SEEK)) render_audio(player->render, aframe);
            }

            packet->data += consumed;
            packet->size -= consumed;
        }
        //-- decode audio packet --//

        // free packet
        av_packet_unref(packet);

        pktqueue_read_enqueue_a(player->pktqueue, packet);
    }

    av_frame_free(&aframe);
    return NULL;
}

static void* video_decode_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    AVFrame  *vframe = NULL;

    vframe = av_frame_alloc();
    if (!vframe) return NULL;
    else vframe->pts = -1;

    while (!(player->player_status & PS_CLOSE))
    {
        //++ when video decode pause ++//
        if (player->player_status & PS_V_PAUSE) {
            player->player_status |= (PS_V_PAUSE << 16);
            usleep(20*1000); continue;
        }
        //-- when video decode pause --//

        // read packet
        packet = pktqueue_read_dequeue_v(player->pktqueue);
        if (packet == NULL) {
            render_video(player->render, vframe);
            usleep(20*1000); continue;
        }

        //++ decode video packet ++//
        while (packet->size > 0 && !(player->player_status & (PS_V_PAUSE|PS_CLOSE))) {
            int consumed = 0;
            int gotvideo = 0;

            consumed = avcodec_decode_video2(player->vcodec_context, vframe, &gotvideo, packet);
            if (consumed < 0) {
                av_log(NULL, AV_LOG_WARNING, "an error occurred during decoding video.\n");
                break;
            }

            if (gotvideo) {
                vfilter_graph_input(player, vframe);
                do {
                    if (vfilter_graph_output(player, vframe) < 0) break;
                    vframe->pts = av_rescale_q(av_frame_get_best_effort_timestamp(vframe), player->vstream_timebase, TIMEBASE_MS);
                    //++ for seek operation
                    if (player->player_status & PS_V_SEEK) {
                        if (player->seek_dest - vframe->pts < 100) {
                            player->player_status &= ~PS_V_SEEK;
                            if (player->player_status & PS_R_PAUSE) {
                                render_pause(player->render);
                            }
                        }
                    }
                    //-- for seek operation
                    if (!(player->player_status & PS_V_SEEK)) render_video(player->render, vframe);
                } while (player->vfilter_graph && player->vfilter_enable);
            }

            packet->data += consumed;
            packet->size -= consumed;
        }
        //-- decode video packet --//

        // free packet
        av_packet_unref(packet);

        pktqueue_read_enqueue_v(player->pktqueue, packet);
    }

    av_frame_free(&vframe);
    return NULL;
}

static int reinit_stream(PLAYER *player, enum AVMediaType type, int sel) {
    AVCodecContext *lastctxt = NULL;
    AVCodec        *decoder  = NULL;
    int             idx = -1, cur = -1;

    for (int i=0; i<(int)player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codec->codec_type == type) {
            idx = i; if (++cur == sel) break;
        }
    }
    if (idx == -1) return -1;

    switch (type) {
    case AVMEDIA_TYPE_AUDIO:
        // get last codec context
        if (player->acodec_context) avcodec_close(player->acodec_context);

        // get new acodec_context & astream_timebase
        player->acodec_context   = player->avformat_context->streams[idx]->codec;
        player->astream_timebase = player->avformat_context->streams[idx]->time_base;

        // reopen codec
        decoder = avcodec_find_decoder(player->acodec_context->codec_id);
        if (decoder && avcodec_open2(player->acodec_context, decoder, NULL) == 0) {
            player->astream_index = idx;
        } else {
            av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for audio !\n");
        }
        break;

    case AVMEDIA_TYPE_VIDEO:
        // get last codec context
        if (player->vcodec_context) avcodec_close(player->vcodec_context);

        // get new vcodec_context & vstream_timebase
        player->vcodec_context   = player->avformat_context->streams[idx]->codec;
        player->vstream_timebase = player->avformat_context->streams[idx]->time_base;

        //++ reopen codec
        //+ try android mediacodec hardware decoder
        if (player->init_params.video_hwaccel) {
            switch (player->vcodec_context->codec_id) {
            case AV_CODEC_ID_H264      : decoder = avcodec_find_decoder_by_name("h264_mediacodec" ); break;
            case AV_CODEC_ID_HEVC      : decoder = avcodec_find_decoder_by_name("hevc_mediacodec" ); break;
            case AV_CODEC_ID_VP8       : decoder = avcodec_find_decoder_by_name("vp8_mediacodec"  ); break;
            case AV_CODEC_ID_VP9       : decoder = avcodec_find_decoder_by_name("vp9_mediacodec"  ); break;
            case AV_CODEC_ID_MPEG2VIDEO: decoder = avcodec_find_decoder_by_name("mpeg2_mediacodec"); break;
            case AV_CODEC_ID_MPEG4     : decoder = avcodec_find_decoder_by_name("mpeg4_mediacodec"); break;
            default: break;
            }
            if (decoder && avcodec_open2(player->vcodec_context, decoder, NULL) == 0) {
                player->vstream_index = idx;
                av_log(NULL, AV_LOG_WARNING, "using android mediacodec hardware decoder %s !\n", decoder->name);
            } else {
                avcodec_close(player->vcodec_context);
                decoder = NULL;
            }
            player->init_params.video_hwaccel = decoder ? 1 : 0;
        }
        //- try android mediacodec hardware decoder

        if (!decoder) {
            // try to set video decoding thread count
            player->vcodec_context->thread_count = player->init_params.video_thread_count;
            decoder = avcodec_find_decoder(player->vcodec_context->codec_id);
            if (decoder && avcodec_open2(player->vcodec_context, decoder, NULL) == 0) {
                player->vstream_index = idx;
            } else {
                av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for video !\n");
            }
            // get the actual video decoding thread count
            player->init_params.video_thread_count = player->vcodec_context->thread_count;
        }
        //-- reopen codec
        break;

    case AVMEDIA_TYPE_SUBTITLE:
        return -1; // todo...
    default:
        return -1;
    }

    return 0;
}

static int get_stream_total(PLAYER *player, enum AVMediaType type) {
    int total, i;
    for (i=0,total=0; i<(int)player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codec->codec_type == type) {
            total++;
        }
    }
    return total;
}

static int get_stream_current(PLAYER *player, enum AVMediaType type) {
    int idx, cur, i;
    switch (type) {
    case AVMEDIA_TYPE_AUDIO   : idx = player->astream_index; break;
    case AVMEDIA_TYPE_VIDEO   : idx = player->vstream_index; break;
    case AVMEDIA_TYPE_SUBTITLE: return -1; // todo...
    default: return -1;
    }
    for (i=0,cur=-1; i<(int)player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codec->codec_type == type) {
            cur++;
        }
        if (i == idx) {
            break;
        }
    }
    return cur;
}

static int interrupt_callback(void *param)
{
    PLAYER *player = (PLAYER*)param;
    if (player->init_timeout == -1) return 0;
    else return av_gettime() - player->init_timetick > player->init_timeout ? AVERROR_EOF : 0;
}

// 函数实现
void* player_open(char *file, void *win, PLAYER_INIT_PARAMS *params)
{
    PLAYER       *player  = NULL;
    int           arate   = 0;
    int           aformat = 0;
    uint64_t      alayout = 0;
    AVRational    vrate   = { 21, 1 };
    AVPixelFormat vformat = AV_PIX_FMT_YUV420P;
    int           width   = 0;
    int           height  = 0;

    //++ for avdevice
    #define AVDEV_DSHOW   "dshow"
    #define AVDEV_GDIGRAB "gdigrab"
    #define AVDEV_VFWCAP  "vfwcap"
    char          *url    = file;
    AVInputFormat *fmt    = NULL;
    //-- for avdevice

    // av register all
    av_register_all();
    avdevice_register_all();
    avfilter_register_all();
    avformat_network_init();

    // setup log
    av_log_set_level(AV_LOG_WARNING);
    av_log_set_callback(ffplayer_log_callback);

    // alloc player context
    player = (PLAYER*)calloc(1, sizeof(PLAYER));

    // for player init params
    if (params) {
        memcpy(&player->init_params, params, sizeof(PLAYER_INIT_PARAMS));
    }

    // create packet queue
    player->pktqueue = pktqueue_create(0);

    //++ for avdevice
    if (strstr(file, AVDEV_DSHOW) == file) {
        fmt = av_find_input_format(AVDEV_DSHOW);
        url = file + strlen(AVDEV_DSHOW) + 3;
    } else if (strstr(file, AVDEV_GDIGRAB) == file) {
        fmt = av_find_input_format(AVDEV_GDIGRAB);
        url = file + strlen(AVDEV_GDIGRAB) + 3;
    } else if (strstr(file, AVDEV_VFWCAP) == file) {
        fmt = av_find_input_format(AVDEV_VFWCAP);
        url = NULL;
    }
    //-- for avdevice

    //++ for player init timeout
    if (player->init_params.init_timeout > 0) {
        player->avformat_context = avformat_alloc_context();
        if (!player->avformat_context) goto error_handler;
        player->avformat_context->interrupt_callback.callback = interrupt_callback;
        player->avformat_context->interrupt_callback.opaque   = player;
        player->init_timetick = av_gettime();
        player->init_timeout  = player->init_params.init_timeout * 1000;
    }
    //-- for player init timeout

    // open input file
    if (avformat_open_input(&player->avformat_context, url, fmt, NULL) != 0) {
        goto error_handler;
    } else {
        player->init_timeout = -1;
    }

    // find stream info
    if (avformat_find_stream_info(player->avformat_context, NULL) < 0) {
        goto error_handler;
    }

    // get start_pts
    if (player->avformat_context->start_time > 0) {
        player->start_pts = player->avformat_context->start_time * 1000 / AV_TIME_BASE;
    }

    // set current audio & video stream
    player->astream_index = -1; reinit_stream(player, AVMEDIA_TYPE_AUDIO, player->init_params.audio_stream_cur);
    player->vstream_index = -1; reinit_stream(player, AVMEDIA_TYPE_VIDEO, player->init_params.video_stream_cur);

    // for audio
    if (player->astream_index != -1)
    {
        arate   = player->acodec_context->sample_rate;
        aformat = player->acodec_context->sample_fmt;
        alayout = player->acodec_context->channel_layout;
        //++ fix audio channel layout issue
        if (alayout == 0) {
            alayout = av_get_default_channel_layout(player->acodec_context->channels);
        }
        //-- fix audio channel layout issue
    }

    // for video
    if (player->vstream_index != -1) {
        vrate = player->avformat_context->streams[player->vstream_index]->r_frame_rate;
        if (vrate.num / vrate.den > 100) {
            vrate.num = 21;
            vrate.den = 1;
        }
        vformat = player->vcodec_context->pix_fmt;
        width   = player->vcodec_context->width;
        height  = player->vcodec_context->height;
    }

    // init avfilter graph
    vfilter_graph_init(player);

    // open render
    player->render = render_open(
        player->init_params.adev_render_type, arate, (AVSampleFormat)aformat, alayout,
        player->init_params.vdev_render_type, win, vrate, vformat, width, height);

    if (player->vstream_index == -1) {
        int effect = VISUAL_EFFECT_WAVEFORM;
        render_setparam(player->render, PARAM_VISUAL_EFFECT, &effect);
    }

    // for player init params
    player->init_params.video_width          = width;
    player->init_params.video_height         = height;
    player->init_params.video_frame_rate     = vrate.num / vrate.den;
    player->init_params.video_stream_total   = get_stream_total(player, AVMEDIA_TYPE_VIDEO);
    player->init_params.video_stream_cur     = player->vstream_index;
    player->init_params.audio_channels       = av_get_channel_layout_nb_channels(alayout);
    player->init_params.audio_sample_rate    = arate;
    player->init_params.audio_stream_total   = get_stream_total(player, AVMEDIA_TYPE_AUDIO);
    player->init_params.audio_stream_cur     = player->astream_index;
    player->init_params.subtitle_stream_total= get_stream_total(player, AVMEDIA_TYPE_SUBTITLE);
    player->init_params.subtitle_stream_cur  = -1;
    if (params) {
        memcpy(params, &player->init_params, sizeof(PLAYER_INIT_PARAMS));
    }

    // make sure player status paused
    player->player_status = (PS_A_PAUSE|PS_V_PAUSE|PS_R_PAUSE);
    pthread_create(&player->avdemux_thread, NULL, av_demux_thread_proc    , player);
    pthread_create(&player->adecode_thread, NULL, audio_decode_thread_proc, player);
    pthread_create(&player->vdecode_thread, NULL, video_decode_thread_proc, player);

    // return
    return player;

error_handler:
    player_close(player);
    return NULL;
}

void player_close(void *hplayer)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    //++ fix noise sound issue when player_close on android platform
    int vol = -255; player_setparam(player, PARAM_AUDIO_VOLUME, &vol);
    //-- fix noise sound issue when player_close on android platform

    // set init_timeout to 0
    player->init_timeout = 0;

    // set close flag
    player->player_status |= PS_CLOSE;
    if (player->render) render_start(player->render);

    // wait audio/video demuxing thread exit
    pthread_join(player->avdemux_thread, NULL);

    // wait audio decoding thread exit
    pthread_join(player->adecode_thread, NULL);

    // wait video decoding thread exit
    pthread_join(player->vdecode_thread, NULL);

    // free avfilter graph
    vfilter_graph_free(player);

    // destroy packet queue
    pktqueue_destroy(player->pktqueue);

    if (player->render          ) render_close (player->render);
    if (player->acodec_context  ) avcodec_close(player->acodec_context);
    if (player->vcodec_context  ) avcodec_close(player->vcodec_context);
    if (player->avformat_context) avformat_close_input(&player->avformat_context);
    if (player->init_params.init_timeout > 0) {
        avformat_free_context(player->avformat_context);
    }

    free(player);

    // deinit network
    avformat_network_deinit();
}

void player_play(void *hplayer)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;
    player->player_status = 0;
    render_start(player->render);
}

void player_pause(void *hplayer)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;
    player->player_status |= PS_R_PAUSE;
    render_pause(player->render);
}

void player_setrect(void *hplayer, int type, int x, int y, int w, int h)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    //++ if set visual effect rect
    if (type == 1) {
        render_setrect(player->render, type, x, y, w, h);
        return;
    }
    //-- if set visual effect rect

    int vw = player->vcodec_context->width ;
    int vh = player->vcodec_context->height;
    int rw = 0, rh = 0;
    if (!vw || !vh) return;

    player->vdrect.left   = x;
    player->vdrect.top    = y;
    player->vdrect.right  = x + w;
    player->vdrect.bottom = y + h;

    switch (player->vdmode)
    {
    case VIDEO_MODE_LETTERBOX:
        if (w * vh < h * vw) { rw = w; rh = rw * vh / vw; }
        else                 { rh = h; rw = rh * vw / vh; }
        break;
    case VIDEO_MODE_STRETCHED: rw = w; rh = h; break;
    }

    if (rw <= 0) rw = 1;
    if (rh <= 0) rh = 1;
    render_setrect(player->render, type, x + (w - rw) / 2, y + (h - rh) / 2, rw, rh);
}

void player_seek(void *hplayer, int64_t ms, int type)
{
    if (!hplayer) return;
    PLAYER *player   = (PLAYER*)hplayer;
    int64_t startpts = 0;

    // get start pts
    player->seek_dest = player->start_pts + ms;
    player->seek_type = type;

    // make render run first
    render_start(player->render);

    // set PS_F_SEEK flag
    player->player_status |=  PS_F_SEEK;
}

int player_snapshot(void *hplayer, char *file, int w, int h, int waitt)
{
    if (!hplayer) return -1;
    PLAYER *player = (PLAYER*)hplayer;
    return player->vstream_index == -1 ? -1 : render_snapshot(player->render, file, w, h, waitt);
}

void player_setparam(void *hplayer, int id, void *param)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_VIDEO_MODE:
        player->vdmode = *(int*)param;
        player_setrect(hplayer, 0,
            player->vdrect.left, player->vdrect.top,
            player->vdrect.right - player->vdrect.left,
            player->vdrect.bottom - player->vdrect.top);
        break;
    case PARAM_VFILTER_ENABLE:
        player->vfilter_enable = *(int*)param;
        break;
    default:
        render_setparam(player->render, id, param);
        break;
    }
}

void player_getparam(void *hplayer, int id, void *param)
{
    if (!hplayer || !param) return;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_MEDIA_DURATION:
        if (!player->avformat_context) *(int64_t*)param = 1;
        else *(int64_t*)param = (player->avformat_context->duration * 1000 / AV_TIME_BASE);
        if (*(int64_t*)param <= 0) *(int64_t*)param = 1;
        break;
    case PARAM_MEDIA_POSITION:
        if (player->player_status & (PS_A_PAUSE|PS_V_PAUSE|PS_F_SEEK|PS_A_SEEK|PS_V_SEEK)) {
            *(int64_t*)param = player->seek_dest - player->start_pts;
        } else {
            int64_t pos = 0; render_getparam(player->render, id, &pos);
            if (pos == -1) {
                *(int64_t*)param = -1;
            } else {
                *(int64_t*)param = pos - player->start_pts < 0 ? 0 : pos - player->start_pts;
            }
        }
        break;
    case PARAM_VIDEO_WIDTH:
        if (!player->vcodec_context) *(int*)param = 0;
        else *(int*)param = player->vcodec_context->width;
        break;
    case PARAM_VIDEO_HEIGHT:
        if (!player->vcodec_context) *(int*)param = 0;
        else *(int*)param = player->vcodec_context->height;
        break;
    case PARAM_VIDEO_MODE:
        *(int*)param = player->vdmode;
        break;
    case PARAM_RENDER_GET_CONTEXT:
        *(void**)param = player->render;
        break;
    case PARAM_VFILTER_ENABLE:
        *(int*)param = player->vfilter_enable;
        break;
    default:
        render_getparam(player->render, id, param);
        break;
    }
}




