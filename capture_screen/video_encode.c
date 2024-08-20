//
// Created by lqx on 2024/8/9.
//
#include "capture_screen.h"
#include <stdbool.h>

// encode ctx
typedef struct {
    bool able;
    int64_t pts;
    AVCodecContext *codec_ctx;
    AVPacket *packet;
    video_encode_result_callback_t func;
} encode_ctx_t;

void *video_encode_start(video_encode_result_callback_t func) {
    int ret = 0;
    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVPacket *packet = NULL;

    codec = avcodec_find_encoder(AV_CODEC_ID_H265);
    if (NULL == codec) {
        fprintf(stderr, "Codec '%s' not found\n", avcodec_get_name(AV_CODEC_ID_H265));
        return NULL;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (NULL == codec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return NULL;
    }
    // 配置编码器
    codec_ctx->bit_rate = 400000;
    codec_ctx->width = 720;
    codec_ctx->height = 360;
    codec_ctx->time_base = (AVRational) {1, 25};
    codec_ctx->framerate = (AVRational) {25, 1};
    codec_ctx->gop_size = 50; // 2s one gop
    codec_ctx->max_b_frames = 0;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // 打开编码器
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        avcodec_free_context(&codec_ctx);
        return NULL;
    }
    packet = av_packet_alloc();
    if (NULL == packet) {
        fprintf(stderr, "av_packet_alloc failed\n");
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
        return NULL;
    }
    // create success
    encode_ctx_t *ctx = calloc(1, sizeof(encode_ctx_t));
    ctx->able = true;
    ctx->pts = 0;
    ctx->codec_ctx = codec_ctx;
    ctx->packet = packet;
    ctx->func = func;

    return ctx;
}

int video_encode_stop(void *ctx) {
    encode_ctx_t *ectx = ctx;
    ectx->able = false;

    AVCodecContext *codec_ctx = ectx->codec_ctx;
    AVPacket *packet = ectx->packet;

    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    av_packet_free(&packet);

    free(ectx);
    return 0;
}

int video_encode_process(void *ctx, struct AVFrame *frame) {
    int ret = 0;
    encode_ctx_t *ectx = ctx;
    if (!ectx->able) 
        return -1;
    
    AVCodecContext *codec_ctx = ectx->codec_ctx;
    AVPacket *packet = ectx->packet;
    frame->pts = (ectx->pts++);
    
    ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return ret;
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, packet);
        if ((ret == AVERROR(EAGAIN)) || (ret == AVERROR_EOF)) {
            return -1;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            return -1;
        }
        printf("Write packet %3" PRId64 " (size=%5d)\n", packet->pts, packet->size);
        // 写入文件
        if (NULL != ectx->func) ectx->func(ctx, packet);
        
        av_packet_unref(packet);
    }
    printf("encode one frame success\n");
    return 0;
}

AVCodecContext *video_encode_get_codec_ctx(void *ctx) {
    encode_ctx_t *ectx = ctx;
    return ectx->codec_ctx; // may be nil
}

