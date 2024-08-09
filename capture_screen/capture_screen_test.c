//
// Created by root on 2024/3/18.
//
#include "capture_screen.h"
#include <unistd.h>
#include <stdio.h>


static FILE *out_file = NULL;
static void *g_codec_ctx  = NULL;
static void *g_cap_ctx = NULL;
static void *g_scale_ctx = NULL;

void handle_video_encode_data(void *ctx, uint8_t *data, size_t data_size) {
    // save to file, h265 nalu stream
    fwrite(data, 1, data_size, out_file);
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
    video_encode_process(g_codec_ctx, frame);
}

// capture screen image -> scale -> encode -> stream -> save file
int main(int argc, char *argv[]) {

    out_file = fopen("./output.h265", "wb");

    g_codec_ctx = video_encode_start(handle_video_encode_data);
    g_scale_ctx = video_scale_start(handle_scale_frame);
    g_cap_ctx = video_capture_start(handle_capture_frame);

    sleep(5);

    video_capture_stop(g_cap_ctx);
    video_scale_stop(g_scale_ctx);
    video_encode_stop(g_codec_ctx);

    fclose(out_file);

    return 0;
}