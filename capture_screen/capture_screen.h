#ifndef SIMPLE_FFMPEG_CAPTURE_SCREEN_H
#define SIMPLE_FFMPEG_CAPTURE_SCREEN_H
#include <stddef.h>
#include <stdint.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

typedef void (*video_frame_callback_t)(void *ctx,AVFrame *frame);
void *video_capture_start(video_frame_callback_t func);
int video_capture_stop(void *ctx);

typedef void (*video_frame_callback_t)(void *ctx,AVFrame *frame);
void *video_scale_start(video_frame_callback_t func);
int video_scale_stop(void *ctx);
int video_scale_process(void *ctx, AVFrame *frame);

typedef void (*video_encode_result_callback_t)(void *ctx, uint8_t *data, size_t data_size);
void *video_encode_start(video_encode_result_callback_t func);
int video_encode_stop(void *ctx);
int video_encode_process(void *ctx, struct AVFrame *frame);


#endif //SIMPLE_FFMPEG_CAPTURE_SCREEN_H
