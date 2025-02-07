//
// Created by root on 2025/2/7.
//
#include "capture_screen.h"
#include "video_filter.h"
#include <unistd.h>
#include <stdio.h>


static FILE *out_file = NULL;
static void *g_codec_ctx  = NULL;
static void *g_cap_ctx = NULL;
static void *g_scale_ctx = NULL;
static void *g_filter_ctx = NULL;

void handle_video_encode_data(void *ctx,  AVPacket *packet) {
    // save to file, h265 nalu stream
    static uint64_t cnt = 0;
    if (cnt ++ % 30 ==0){
        LOGI("encode finish data cnt %llu, %d bytes\n", cnt, packet->size);
    }
    fwrite(packet->data, 1, packet->size, out_file);
    fflush(out_file);
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
    filter_process_frame(g_filter_ctx, frame);
}

void handle_filter_cb(void *ctx, AVFrame *frame) {
    // encode
    static uint64_t cnt = 0;
    if (cnt++ % 100 == 0) {
        LOGI("filter finish frame %lu\n", cnt);
    }
    video_encode_process(g_codec_ctx, frame);
}

// capture screen image -> scale -> encode -> stream -> save file
int main(int argc, char *argv[]) {

    out_file = fopen("./output_with_unshape.h265", "wb");

    filter_init(&g_filter_ctx, AV_PIX_FMT_YUV420P, 720, 360, handle_filter_cb);

    g_codec_ctx = video_encode_start(handle_video_encode_data);
    g_scale_ctx = video_scale_start(handle_scale_frame);
    g_cap_ctx = video_capture_start(handle_capture_frame);

    sleep(5);

    video_capture_stop(g_cap_ctx);
    video_scale_stop(g_scale_ctx);
    video_encode_stop(g_codec_ctx);

    filter_deinit(g_filter_ctx);
    fflush(out_file);
    fclose(out_file);


    return 0;
}