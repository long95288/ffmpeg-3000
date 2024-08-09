//
// Created by lqx on 2024/8/9.
//
#include "capture_screen.h"
#include <stdbool.h>

#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"

// scale ctx
typedef struct {
    bool able;
    struct SwsContext *sws_ctx;
    AVFrame *result;
    video_frame_callback_t func;
} scale_ctx_t;

void *video_scale_start(video_frame_callback_t func) {
    // 缩放目标
    AVFrame *result = av_frame_alloc();
    result->format = AV_PIX_FMT_YUV420P; // 缩放到h265编码的目标
    result->width = 720;
    result->height = 360;
    av_frame_get_buffer(result, 0);
    // sws ctx need proccess 
    scale_ctx_t *ctx = calloc(1, sizeof(scale_ctx_t));
    ctx->result = result;
    ctx->able = true;
    ctx->func = func;
    return ctx;
}

int video_scale_stop(void *ctx) {
    scale_ctx_t *sctx = ctx;
    sctx->able = false;
    struct SwsContext *sws_ctx = sctx->sws_ctx;
    AVFrame *result = sctx->result;
    sws_freeContext(sws_ctx);
    av_frame_free(&result);
    free(sctx);
    return 0;
}

int video_scale_process(void *ctx, AVFrame *frame) {
    int ret = 0;
    scale_ctx_t *sctx = ctx;
    if (!sctx->able)
        return -1;
    struct AVFrame *result = sctx->result;
    struct SwsContext *sws_ctx = sctx->sws_ctx;
    if (NULL == sws_ctx) {
        sws_ctx = sws_getContext(
                // input
                frame->width,
                frame->height,
                frame->format,
                // output
                result->width,
                result->height,
                result->format, SWS_BILINEAR,
                NULL, NULL, NULL
        );
        sctx->sws_ctx = sws_ctx;
        printf("sws %s:%dx%d => %s:%dx%d",
                // input info
               av_get_pix_fmt_name(frame->format),
               frame->width, frame->height,
                // output info
               av_get_pix_fmt_name(result->format),
               result->width, result->height
        );
    }
    if (NULL == sws_ctx)
        fprintf(stderr, "sws_getContext error\n");
    
    ret = sws_scale(sws_ctx,
            /*input*/
                    (const uint8_t *const *) frame->data, frame->linesize, 0, frame->height,
            /*ouput*/
                    result->data, result->linesize
    );
    if (ret != result->height) {
        printf("sws_scale error\n");
    }
    if (NULL != sctx->func) sctx->func(sctx, result);
    return 0;
}
