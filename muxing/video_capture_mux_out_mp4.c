//
// Created by root on 2024/8/20.
//
// this test is force to muxing h265 to mp4
//
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include "capture_screen.h"
#include <unistd.h>

#define STREAM_FRAME_RATE 25 /* 25 images/s */

static void *g_codec_ctx  = NULL;
static void *g_cap_ctx = NULL;
static void *g_scale_ctx = NULL;

static AVFormatContext *oc = NULL;
static AVStream *st;

void handle_video_encode_data(void *ctx, AVPacket *packet) {
    AVCodecContext *codec_ctx = video_encode_get_codec_ctx(ctx);
    // 需要时间转换,要不然封装出现异常
    av_packet_rescale_ts(packet,codec_ctx->time_base, st->time_base);
//    packet->stream_index = 0;
    printf(" stream_index:%d pts: %s\n",packet->stream_index, av_ts2str(packet->pts));
    int ret =av_interleaved_write_frame(oc, packet);
    if (ret < 0) {
        fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
        exit(1);
    }
}


void handle_capture_frame(void *ctx, AVFrame *frame) {
    // process frame scale
    static uint64_t count = 0;
    if (count++% 100 == 0) {
        printf("frame %lu\n", count);
    }
    video_scale_process(g_scale_ctx, frame);
}

void handle_scale_frame(void *ctx, AVFrame *frame) {
    // process encode video frame
    static uint64_t count = 0;
    if (count++% 100 == 0) {
        printf("scale frame %lu\n", count);
    }
    video_encode_process(g_codec_ctx, frame);
}

int main(int argc,char *argv[]) {
    //

    int ret;
    const AVOutputFormat *fmt;
    char *filename = "output.mp4";

    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc)
        return 1;

    fmt = oc->oformat;

    // muxing mp4, only h265 stream
    // 1. add hev video stream

    st = avformat_new_stream(oc, NULL);
    st->id = oc->nb_streams -1;
    st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };

    // 配置: 应该从编码器中获取,但是现在编码器在后面才启动的
    AVCodecContext *codec_ctx = NULL;
    codec_ctx = avcodec_alloc_context3(avcodec_find_encoder(AV_CODEC_ID_H265));
    codec_ctx->codec_id = AV_CODEC_ID_H265;
    codec_ctx->bit_rate = 400000;
    codec_ctx->width = 720;
    codec_ctx->height = 360;
    codec_ctx->time_base = (AVRational) {1, 25};
    codec_ctx->framerate = (AVRational) {25, 1};
    codec_ctx->gop_size = 50; // 2s one gop
    codec_ctx->max_b_frames = 0;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(st->codecpar, codec_ctx);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
    avcodec_free_context(&codec_ctx);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename,
                    av_err2str(ret));
            return 1;
        }
    }
    AVDictionary *opt = NULL;
    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        return 1;
    }

    g_codec_ctx = video_encode_start(handle_video_encode_data);
    g_scale_ctx = video_scale_start(handle_scale_frame);
    g_cap_ctx = video_capture_start(handle_capture_frame);

    sleep(5);

    video_capture_stop(g_cap_ctx);
    video_scale_stop(g_scale_ctx);
    video_encode_stop(g_codec_ctx);

    av_write_trailer(oc);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);

    return 0;
}