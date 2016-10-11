// 包含头文件
#include "ffspliter.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 预编译开关
#define FFSPLITER_TEST

// 内部常量定义
static const AVRational TIMEBASE_MS = { 1, 1000 };

// 函数实现
int split_media_file(char *dst, char *src, int start, int end, PFN_SPC spc)
{
    AVOutputFormat  *ofmt      = NULL;
    AVFormatContext *ifmt_ctx  = NULL;
    AVFormatContext *ofmt_ctx  = NULL;
    int64_t         *start_dts = NULL;
    int             *end_flags = NULL;
    int              end_split =  0;
    int              ret       = -1;
    unsigned         i;

    av_register_all();

    if ((ret = avformat_open_input(&ifmt_ctx, src, 0, 0)) < 0) {
        fprintf(stderr, "could not open input file '%s'", src);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "failed to retrieve input stream information");
        goto end;
    }

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, dst);
    if (!ofmt_ctx) {
        fprintf(stderr, "could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;

    for (i=0; i<ifmt_ctx->nb_streams; i++) {
        AVStream *istream = ifmt_ctx->streams[i];
        AVStream *ostream = avformat_new_stream(ofmt_ctx, istream->codec->codec);
        if (!ostream) {
            fprintf(stderr, "failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        ret = avcodec_copy_context(ostream->codec, istream->codec);
        if (ret < 0) {
            fprintf(stderr, "failed to copy context from input to output stream codec context\n");
            goto end;
        }

        ostream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            ostream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, dst, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "could not open output file '%s'", dst);
            goto end;
        }
    }

    // seek to start position
    av_seek_frame(ifmt_ctx, -1, start * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);

    //+ init start_dts & end_flags
    start_dts = (int64_t*)malloc(ifmt_ctx->nb_streams * sizeof(int64_t));
    end_flags = (int    *)malloc(ifmt_ctx->nb_streams * sizeof(int    ));
    if (!start_dts || !end_flags) {
        fprintf(stderr, "failed to allocate memory for start_dts & end_flags !\n");
        ret = -1;
        goto end;
    }
    for (i=0; i<ifmt_ctx->nb_streams; i++) start_dts[i] = end_flags[i] = -1;
    //- init start_dts & end_flags

    // write header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "error occurred when opening output file\n");
        goto end;
    }

    while (1) {
        AVStream *istream, *ostream;
        AVPacket  pkt;
        int64_t   dts;

        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
//          fprintf(stderr, "failed to read frame !\n");
            break;
        }

        istream = ifmt_ctx->streams[pkt.stream_index];
        ostream = ofmt_ctx->streams[pkt.stream_index];

        //+ check split end
        dts = av_rescale_q(pkt.dts, istream->time_base, TIMEBASE_MS);
        if (dts > end * 1000) end_flags[pkt.stream_index] = 0;
        for (i=0, end_split=0; i<ifmt_ctx->nb_streams; i++) end_split += end_flags[i];
        if (end_split == 0) break;
        if (spc && pkt.stream_index == 0) spc(dts);
        //- check split end

        // calculate start_dts
        if (start_dts[pkt.stream_index] == -1) start_dts[pkt.stream_index] = pkt.dts;

        // adjust pts & dts
        if (start_dts[pkt.stream_index] != -1) {
            pkt.pts -= start_dts[pkt.stream_index];
            pkt.dts -= start_dts[pkt.stream_index];
        }

        //+ copy packet
        pkt.pts = av_rescale_q_rnd(pkt.pts, istream->time_base, ostream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, istream->time_base, ostream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, istream->time_base, ostream->time_base);
        pkt.pos = -1;
        //- copy packet

        //+ write frame
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            fprintf(stderr, "error muxing packet\n");
            break;
        }
        //- write frame

        av_packet_unref(&pkt);
    }

    // write trailer
    av_write_trailer(ofmt_ctx);

end:
    if (start_dts) free(start_dts);
    if (end_flags) free(end_flags);

    // close input
    avformat_close_input(&ifmt_ctx);

    // close output
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);

    return ret;
}

#ifdef FFSPLITER_TEST
static void split_progress_callback(__int64 cur)
{
    printf("\rsplit progress: %d", cur / 1000);
}

int main(int argc, char *argv[])
{
    char *input, *output;
    int   start,  end;

#if 1
    if (argc < 5) {
        printf("ffspliter: tools for spliter media file\n"
               "usage: ffspliter input start end output\n"
        );
        return -1;
    }

    input  = argv[1];
    output = argv[4];
    start  = atoi(argv[2]);
    end    = atoi(argv[3]);
#else
    input  = "test.mpg";
    output = "test-1min-to-2min.mpg";
    start  = 60;
    end    = 120;
#endif

    return split_media_file(output, input, start, end, split_progress_callback);
}
#endif

