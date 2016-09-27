// 包含头文件
#include "snapshot.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

// 内部类型定义
typedef struct
{
    struct SwsContext *sws_ctx;
    AVPixelFormat      ifmt;
    AVPixelFormat      ofmt;
    int                width;
    int                height;
    int                scale;
    AVFrame            picture;
} SNAPSHOT_CONTEXT;

// 内部函数实现
static void alloc_picture(AVFrame *picture, enum AVPixelFormat pix_fmt, int width, int height)
{
    int ret;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        printf("could not allocate frame data.\n");
        exit(1);
    }
}

// 函数实现
void* snapshot_init(void)
{
    // allocate context for ffencoder
    SNAPSHOT_CONTEXT *encoder = (SNAPSHOT_CONTEXT*)calloc(1, sizeof(SNAPSHOT_CONTEXT));
    if (!encoder) {
        return NULL;
    }

    // init variables
    encoder->ofmt  = AV_PIX_FMT_YUV420P;
    encoder->scale = SWS_FAST_BILINEAR ;

    // init ffmpeg library
    av_register_all();

    return encoder;
}

int snapshot_take(void *ctxt, char *file, AVFrame *video)
{
    SNAPSHOT_CONTEXT *encoder = (SNAPSHOT_CONTEXT*)ctxt;

    // check valid
    if (!ctxt) return -1;

    if (  encoder->ifmt   != video->format
       || encoder->width  != video->width
       || encoder->height != video->height ) {

        if (encoder->sws_ctx) {
            sws_freeContext(encoder->sws_ctx);
        }
        encoder->sws_ctx = sws_getContext(video->width, video->height, (AVPixelFormat)video->format,
            video->width, video->height, encoder->ofmt, encoder->scale, NULL, NULL, NULL);
        if (!encoder->sws_ctx) {
            printf("could not initialize the conversion context jpg\n");
            exit(1);
        }

        if (video->width != encoder->width || video->height != encoder->height) {
            // free jpeg picture frame
            av_frame_unref(&encoder->picture);

            // alloc picture
            alloc_picture(&encoder->picture, encoder->ofmt, video->width, video->height);
        }

        encoder->ifmt   = (AVPixelFormat)video->format;
        encoder->width  = video->width;
        encoder->height = video->height;
    }

    // scale picture
    sws_scale(
        encoder->sws_ctx,
        video->data,
        video->linesize,
        0, encoder->height,
        encoder->picture.data,
        encoder->picture.linesize);

    AVFormatContext *fmt_ctxt   = NULL;
    AVOutputFormat  *out_fmt    = NULL;
    AVStream        *stream     = NULL;
    AVCodecContext  *codec_ctxt = NULL;
    AVCodec         *codec      = NULL;
    AVPacket         packet     = {};
    int              got        = 0;
    int              ret        = 0;

    fmt_ctxt = avformat_alloc_context();
    out_fmt  = av_guess_format("mjpeg", NULL, NULL);
    fmt_ctxt->oformat = out_fmt;
    if (avio_open(&fmt_ctxt->pb, file, AVIO_FLAG_READ_WRITE) < 0) {
        printf("failed to open output file: %s !\n", file);
        goto done;
    }

    stream = avformat_new_stream(fmt_ctxt, 0);
    if (!stream) {
        printf("failed to add stream !\n");
        goto done;
    }

    codec_ctxt                = stream->codec;
    codec_ctxt->codec_id      = out_fmt->video_codec;
    codec_ctxt->codec_type    = AVMEDIA_TYPE_VIDEO;
    codec_ctxt->pix_fmt       = AV_PIX_FMT_YUVJ420P;
    codec_ctxt->width         = encoder->width;
    codec_ctxt->height        = encoder->height;
    codec_ctxt->time_base.num = 1;
    codec_ctxt->time_base.den = 25;

    codec = avcodec_find_encoder(codec_ctxt->codec_id);
    if (!codec) {
        printf("failed to find encoder !\n");
        goto done;
    }

    if (avcodec_open2(codec_ctxt, codec, NULL) < 0) {
        printf("failed to open encoder !\n");
        goto done;
    }

    avcodec_encode_video2(codec_ctxt, &packet, &encoder->picture, &got);
    if (got) {
        ret = avformat_write_header(fmt_ctxt, NULL);
        if (ret < 0) {
            printf("error occurred when opening output file !\n");
            goto done;
        }
        av_write_frame(fmt_ctxt, &packet);
        av_write_trailer(fmt_ctxt);
    }

done:
    if (codec_ctxt  ) avcodec_close(codec_ctxt);
    if (fmt_ctxt->pb) avio_close(fmt_ctxt->pb);
    if (fmt_ctxt    ) avformat_free_context(fmt_ctxt);
    av_packet_unref(&packet);

    return 0;
}

void snapshot_free(void *ctxt)
{
    SNAPSHOT_CONTEXT *encoder = (SNAPSHOT_CONTEXT*)ctxt;
    if (!ctxt) return;

    // free jpeg picture frame
    av_frame_unref(&encoder->picture);

    // free sws context
    sws_freeContext(encoder->sws_ctx);

    // free snapshot context
    free(ctxt);
}

