// 包含头文件
#include <pthread.h>
#include "pktqueue.h"
#include "ffrender.h"
#include "ffplayer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
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

    // thread
    #define PS_D_PAUSE    (1 << 0)  // demux pause
    #define PS_A_PAUSE    (1 << 1)  // audio decoding pause
    #define PS_V_PAUSE    (1 << 2)  // video decoding pause
    #define PS_R_PAUSE    (1 << 3)  // rendering pause
    #define PS_A_SEEK     (1 << 4)  // seek audio
    #define PS_V_SEEK     (1 << 5)  // seek video
    #define PS_CLOSE      (1 << 6)  // close player
    #define PAUSE_REQ ((PS_D_PAUSE|PS_A_PAUSE|PS_V_PAUSE) << 0 )
    #define PAUSE_ACK ((PS_D_PAUSE|PS_A_PAUSE|PS_V_PAUSE) << 16)
    int              player_status;
    int64_t          seek_dest_pts;

    pthread_t        avdemux_thread;
    pthread_t        adecode_thread;
    pthread_t        vdecode_thread;
} PLAYER;

// 内部常量定义
static const AVRational TIMEBASE_MS = { 1, 1000 };

// 内部函数实现
static void ffplayer_log_callback(void* ptr, int level, const char *fmt, va_list vl) {
    if (level <= av_log_get_level()) {
        char str[1024];
        vsprintf_s(str, 1024, fmt, vl);
        OutputDebugStringA(str);
    }
}

static void* av_demux_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    int       retv   = 0;

    while (!(player->player_status & PS_CLOSE))
    {
        //++ when demux pause ++//
        if (player->player_status & PS_D_PAUSE) {
            player->player_status |= (PS_D_PAUSE << 16);
            usleep(20*1000); continue;
        }
        //-- when demux pause --//

        packet = pktqueue_write_request(player->pktqueue);
        if (packet == NULL) { usleep(20*1000); continue; }

        retv = av_read_frame(player->avformat_context, packet);
        //++ play completed ++//
        if (retv < 0) {
            player->player_status |= PS_D_PAUSE;
            pktqueue_write_post_i(player->pktqueue, packet);
            usleep(20*1000); continue;
        }
        //-- play completed --//

        // audio
        if (packet->stream_index == player->astream_index)
        {
            pktqueue_write_post_a(player->pktqueue, packet);
        }

        // video
        if (packet->stream_index == player->vstream_index)
        {
            pktqueue_write_post_v(player->pktqueue, packet);
        }

        if (  packet->stream_index != player->astream_index
           && packet->stream_index != player->vstream_index )
        {
            av_packet_unref(packet); // free packet
            pktqueue_write_post_i(player->pktqueue, packet);
        }
    }

    return NULL;
}

static void* audio_decode_thread_proc(void *param)
{
    PLAYER   *player = (PLAYER*)param;
    AVPacket *packet = NULL;
    AVFrame  *aframe = NULL;

    aframe = av_frame_alloc();
    if (!aframe) return NULL;

    while (!(player->player_status & PS_CLOSE))
    {
        //++ when audio decode pause ++//
        if (player->player_status & PS_A_PAUSE) {
            player->player_status |= (PS_A_PAUSE << 16);
            usleep(20*1000); continue;
        }
        //-- when audio decode pause --//

        //++ for seek operation
        if (player->player_status & (PS_A_SEEK << 16)) {
            usleep(20*1000); continue;
        }
        //++ for seek operation

        // read packet
        packet = pktqueue_read_request_a(player->pktqueue);
        if (packet == NULL) { usleep(20*1000); continue; }

        //++ decode audio packet ++//
        if (player->astream_index != -1) {
            while (packet->size > 0) {
                int consumed = 0;
                int gotaudio = 0;

                consumed = avcodec_decode_audio4(player->acodec_context, aframe, &gotaudio, packet);
                if (consumed < 0) {
                    av_log(NULL, AV_LOG_WARNING, "an error occurred during decoding audio.\n");
                    break;
                }

                if (gotaudio) {
                    aframe->pts = av_rescale_q(av_frame_get_best_effort_timestamp(aframe), player->astream_timebase, TIMEBASE_MS);
                    //++ for seek operation
                    if ((player->player_status & PS_A_SEEK)) {
                        if (player->seek_dest_pts - aframe->pts < 100) {
                            player->player_status |= (PS_A_SEEK << 16);
                        }
                    }
                    //-- for seek operation
                    else render_audio(player->render, aframe);
                }

                packet->data += consumed;
                packet->size -= consumed;
            }
        }
        //-- decode audio packet --//

        // free packet
        av_packet_unref(packet);

        pktqueue_read_done_a(player->pktqueue, packet);
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

        //++ for seek operation
        if (player->player_status & (PS_V_SEEK << 16)) {
            usleep(20*1000); continue;
        }
        //++ for seek operation

        // read packet
        packet = pktqueue_read_request_v(player->pktqueue);
        if (packet == NULL) {
            if ((player->player_status & PS_V_SEEK)) {
                player->player_status |= (PS_V_SEEK << 16);
            }
            else render_video(player->render, vframe);
            usleep(20*1000); continue;
        }

        //++ decode video packet ++//
        if (player->vstream_index != -1) {
            while (packet->size > 0) {
                int consumed = 0;
                int gotvideo = 0;

                consumed = avcodec_decode_video2(player->vcodec_context, vframe, &gotvideo, packet);
                if (consumed < 0) {
                    av_log(NULL, AV_LOG_WARNING, "an error occurred during decoding video.\n");
                    break;
                }

                if (gotvideo) {
                    vframe->pts = av_rescale_q(av_frame_get_best_effort_timestamp(vframe), player->vstream_timebase, TIMEBASE_MS);
                    //++ for seek operation
                    if ((player->player_status & PS_V_SEEK)) {
                        if (player->seek_dest_pts - vframe->pts < 100) {
                            player->player_status |= (PS_V_SEEK << 16);
                        }
                    }
                    //-- for seek operation
                    else render_video(player->render, vframe);
                }

                packet->data += consumed;
                packet->size -= consumed;
            }
        }
        //-- decode video packet --//

        // free packet
        av_packet_unref(packet);

        pktqueue_read_done_v(player->pktqueue, packet);
    }

    av_frame_free(&vframe);
    return NULL;
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

static int reinit_stream(PLAYER *player, enum AVMediaType type, int sel) {
    AVCodecContext *lastctxt = NULL;
    AVCodec        *decoder  = NULL;
    int             idx, cur, i;

    for (i=0,idx=-1,cur=-1; i<(int)player->avformat_context->nb_streams; i++) {
        if (player->avformat_context->streams[i]->codec->codec_type == type) {
            cur++;
        }
        if (cur == sel) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return -1;

    switch (type) {
    case AVMEDIA_TYPE_AUDIO:
        // get last codec context
        if (player->acodec_context) {
            lastctxt = player->acodec_context;
        }

        // get new acodec_context & astream_timebase
        player->acodec_context   = player->avformat_context->streams[idx]->codec;
        player->astream_timebase = player->avformat_context->streams[idx]->time_base;

        // reopen codec
        if (lastctxt) avcodec_close(lastctxt);
        decoder = avcodec_find_decoder(player->acodec_context->codec_id);
        if (decoder && avcodec_open2(player->acodec_context, decoder, NULL) == 0) {
            player->astream_index = idx;
        }
        else {
            av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for audio !\n");
            player->astream_index = -1;
        }
        break;

    case AVMEDIA_TYPE_VIDEO:
        // get last codec context
        if (player->vcodec_context) {
            lastctxt = player->vcodec_context;
        }

        // get new vcodec_context & vstream_timebase
        player->vcodec_context   = player->avformat_context->streams[idx]->codec;
        player->vstream_timebase = player->avformat_context->streams[idx]->time_base;

        // reopen codec
        if (lastctxt) avcodec_close(lastctxt);
        decoder = avcodec_find_decoder(player->vcodec_context->codec_id);
        if (decoder && avcodec_open2(player->vcodec_context, decoder, NULL) == 0) {
            player->vstream_index = idx;
        }
        else {
            av_log(NULL, AV_LOG_WARNING, "failed to find or open decoder for video !\n");
            player->vstream_index = -1;
        }
        break;

    case AVMEDIA_TYPE_SUBTITLE:
        return -1; // todo...
    }

    return 0;
}

static void make_player_thread_pause(PLAYER *player, int pause) {
    if (pause) {
        // make player thread paused
        player->player_status |= PAUSE_REQ;
        player->player_status &=~PAUSE_ACK;
        render_start(player->render);
        while ((player->player_status & PAUSE_ACK) != PAUSE_ACK) usleep(20*1000);
    }
    else {
        // make player thread run
        player->player_status &=~(PAUSE_REQ | PAUSE_ACK);
        // pause render if needed
        if (player->player_status & PS_R_PAUSE) {
            render_pause(player->render);
        }
    }
}

static void set_stream_current(PLAYER *player, enum AVMediaType type, int sel) {
    LONGLONG             pos = 0;
    RENDER_UPDATE_PARAMS params;

    // get current play posistion
    player_getparam(player, PARAM_MEDIA_POSITION, &pos);

    // pause player threads
    make_player_thread_pause(player, 1);

    // reinit stream
    reinit_stream(player, type, sel);

    // update render
    params.samprate = player->acodec_context->sample_rate;
    params.sampfmt  = player->acodec_context->sample_fmt;
    params.chlayout = player->acodec_context->channel_layout;
    params.frate    = player->avformat_context->streams[player->vstream_index]->r_frame_rate;
    if (params.frate.num / params.frate.den > 100) {
        params.frate.num = 21;
        params.frate.den = 1;
    }
    params.pixfmt   = player->vcodec_context->pix_fmt;
    params.width    = player->vcodec_context->width;
    params.height   = player->vcodec_context->height;
    render_setparam(player->render, PARAM_RENDER_UPDATE, &params);

    // seek current pos to update pktqueue
    player_seek(player, pos);

    // resume player threads
//  make_player_thread_pause(player, 0); // no need to call this function
                                         // player_seek already resume all player threads
}

// 函数实现
void* player_open(char *file, void *win)
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
    char          *avdev_dshow   = "dshow";
    char          *avdev_gdigrab = "gdigrab";
    char          *avdev_vfwcap  = "vfwcap";
    char          *url           = file;
    AVInputFormat *fmt           = NULL;
    //-- for avdevice

    // av register all
    av_register_all();
    avdevice_register_all();

    // setup log
    av_log_set_level(AV_LOG_WARNING);
    av_log_set_callback(ffplayer_log_callback);

    // alloc player context
    player = (PLAYER*)calloc(1, sizeof(PLAYER));

    // create packet queue
    player->pktqueue = pktqueue_create(0);

    //++ for avdevice
    if (strstr(file, avdev_dshow) == file) {
        fmt = av_find_input_format(avdev_dshow);
        url = file + strlen(avdev_dshow) + 3;
    }
    else if (strstr(file, avdev_gdigrab) == file) {
        fmt = av_find_input_format(avdev_gdigrab);
        url = file + strlen(avdev_gdigrab) + 3;
    }
    else if (strstr(file, avdev_vfwcap) == file) {
        fmt = av_find_input_format(avdev_vfwcap);
        url = NULL;
    }
    //-- for avdevice

    // open input file
    AVDictionary *options = NULL;
//  av_dict_set(&options, "rtsp_transport", "tcp", 0);
    if (avformat_open_input(&player->avformat_context, url, fmt, &options) != 0) {
        goto error_handler;
    }

    // find stream info
    if (avformat_find_stream_info(player->avformat_context, NULL) < 0) {
        goto error_handler;
    }

    // set current audio & video stream
    player->astream_index = -1; reinit_stream(player, AVMEDIA_TYPE_AUDIO, 0);
    player->vstream_index = -1; reinit_stream(player, AVMEDIA_TYPE_VIDEO, 0);

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

    // open render
    player->render = render_open(ADEV_RENDER_TYPE_WAVEOUT, arate, (AVSampleFormat)aformat, alayout,
                                 VDEV_RENDER_TYPE_GDI, win, vrate, vformat, width, height);
    if (player->vstream_index == -1) {
        int effect = VISUAL_EFFECT_WAVEFORM;
        render_setparam(player->render, PARAM_VISUAL_EFFECT, &effect);
    }

    // make sure player status paused
    player->player_status = (PS_D_PAUSE|PS_A_PAUSE|PS_V_PAUSE);
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

    player->player_status |= PS_CLOSE;
    if (player->render) {
        render_start(player->render);
    }

    // wait audio/video demuxing thread exit
    pthread_join(player->avdemux_thread, NULL);

    // wait audio decoding thread exit
    pthread_join(player->adecode_thread, NULL);

    // wait video decoding thread exit
    pthread_join(player->vdecode_thread, NULL);

    // destroy packet queue
    pktqueue_destroy(player->pktqueue);

    if (player->render          ) render_close (player->render);
    if (player->vcodec_context  ) {
        avcodec_close(player->vcodec_context);
    }
    if (player->acodec_context  ) avcodec_close(player->acodec_context);
    if (player->avformat_context) avformat_close_input(&player->avformat_context);

    free(player);
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

    int vw, vh;
    int rw, rh;
    int mode;
    player_getparam(hplayer, PARAM_VIDEO_WIDTH , &vw  );
    player_getparam(hplayer, PARAM_VIDEO_HEIGHT, &vh  );
    player_getparam(hplayer, PARAM_VIDEO_MODE  , &mode);
    if (!vw || !vh) return;

    player->vdrect.left   = x;
    player->vdrect.top    = y;
    player->vdrect.right  = x + w;
    player->vdrect.bottom = y + h;

    switch (mode)
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

void player_seek(void *hplayer, LONGLONG ms)
{
    if (!hplayer) return;
    PLAYER *player   = (PLAYER*)hplayer;
    int64_t startpts = 0;

    // get start pts
    render_getparam(player->render, PARAM_RENDER_START_PTS, &startpts);
    ms += startpts;

    // pause demuxing & decoding threads
    make_player_thread_pause(player, 1);

    // seek frame
    av_seek_frame(player->avformat_context, -1, ms / 1000 * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
    if (player->astream_index != -1) avcodec_flush_buffers(player->acodec_context);
    if (player->vstream_index != -1) avcodec_flush_buffers(player->vcodec_context);

    // reset packet queue
    pktqueue_reset(player->pktqueue);

    // reset render
    render_reset   (player->render);
    render_setparam(player->render, PARAM_MEDIA_POSITION, &ms);

    // restart all thread and render
    int SEEK_REQ = 0;
    int SEEK_ACK = 0;
    int timeout  = 100;
    if (player->astream_index != -1) { SEEK_REQ |= PS_A_SEEK; SEEK_ACK |= PS_A_SEEK << 16; }
    if (player->vstream_index != -1) { SEEK_REQ |= PS_V_SEEK; SEEK_ACK |= PS_V_SEEK << 16; }
    player->seek_dest_pts  = ms;
    player->player_status |= SEEK_REQ;
    player->player_status &=~(SEEK_ACK | PAUSE_REQ | PAUSE_ACK);
    while ( !(player->player_status & (PS_D_PAUSE << 16))
          && (player->player_status & SEEK_ACK) != SEEK_ACK
          && --timeout) {
        usleep(20*1000);
    }
    player->player_status &= ~(SEEK_REQ | SEEK_ACK);

    // resume demuxing & decoding threads
    make_player_thread_pause(player, 0);
}

int player_snapshot(void *hplayer, char *file, int w, int h, int waitt)
{
    if (!hplayer) return -1;
    PLAYER *player = (PLAYER*)hplayer;

    // check video stream exsits
    if (player->vstream_index == -1) {
        return -1;
    }

    return render_snapshot(player->render, file, w, h, waitt);
}

void player_setparam(void *hplayer, DWORD id, void *param)
{
    if (!hplayer) return;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_VIDEO_MODE:
        render_setparam(player->render, PARAM_VIDEO_MODE, param);
        player_setrect(hplayer, 0,
            player->vdrect.left, player->vdrect.top,
            player->vdrect.right - player->vdrect.left,
            player->vdrect.bottom - player->vdrect.top);
        break;
    case PARAM_DECODE_THREAD_COUNT:
        make_player_thread_pause(player, 1);
        player->vcodec_context->thread_count = *(int*)param;
        reinit_stream(player, AVMEDIA_TYPE_VIDEO, player->vstream_index);
        make_player_thread_pause(player, 0);
        break;
    case PARAM_VDEV_RENDER_TYPE:
        make_player_thread_pause(player, 1);
        render_setparam(player->render, PARAM_VDEV_RENDER_TYPE, param);
        make_player_thread_pause(player, 0);
        break;
    case PARAM_AUDIO_STREAM_CUR   : set_stream_current(player, (AVMediaType)AVMEDIA_TYPE_AUDIO   , *(int*)param); break;
    case PARAM_VIDEO_STREAM_CUR   : set_stream_current(player, (AVMediaType)AVMEDIA_TYPE_VIDEO   , *(int*)param); break;
    case PARAM_SUBTITLE_STREAM_CUR: set_stream_current(player, (AVMediaType)AVMEDIA_TYPE_SUBTITLE, *(int*)param); break;
    default:
        render_setparam(player->render, id, param);
        break;
    }
}

void player_getparam(void *hplayer, DWORD id, void *param)
{
    if (!hplayer || !param) return;
    PLAYER *player = (PLAYER*)hplayer;

    switch (id)
    {
    case PARAM_DECODE_THREAD_COUNT:
        if (!player->vcodec_context) *(int*)param = 0;
        else *(int*)param = player->vcodec_context->thread_count;
        break;
    case PARAM_MEDIA_DURATION:
        if (!player->avformat_context) *(int64_t*)param = 1;
        else *(int64_t*)param = (player->avformat_context->duration * 1000 / AV_TIME_BASE);
        if (*(int64_t*)param == 0) *(int64_t*)param = 1;
        break;
    case PARAM_VIDEO_WIDTH:
        if (!player->vcodec_context) *(int*)param = 0;
        else *(int*)param = player->vcodec_context->width;
        break;
    case PARAM_VIDEO_HEIGHT:
        if (!player->vcodec_context) *(int*)param = 0;
        else *(int*)param = player->vcodec_context->height;
        break;
    case PARAM_AUDIO_STREAM_TOTAL:
        *(int*)param = get_stream_total(player, AVMEDIA_TYPE_AUDIO);
        break;
    case PARAM_VIDEO_STREAM_TOTAL:
        *(int*)param = get_stream_total(player, AVMEDIA_TYPE_VIDEO);
        break;
    case PARAM_SUBTITLE_STREAM_TOTAL:
        *(int*)param = get_stream_total(player, AVMEDIA_TYPE_SUBTITLE);
        break;
    case PARAM_AUDIO_STREAM_CUR:
        *(int*)param = get_stream_current(player, AVMEDIA_TYPE_AUDIO);
        break;
    case PARAM_VIDEO_STREAM_CUR:
        *(int*)param = get_stream_current(player, AVMEDIA_TYPE_VIDEO);
        break;
    case PARAM_SUBTITLE_STREAM_CUR:
        *(int*)param = get_stream_current(player, AVMEDIA_TYPE_SUBTITLE);
        break;
    default:
        render_getparam(player->render, id, param);
        break;
    }
}




