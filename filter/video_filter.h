//
// Created by root on 2025/2/7.
//

#ifndef FFMPEG_3000_VIDEO_FILTER_H
#define FFMPEG_3000_VIDEO_FILTER_H

#include <stdio.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <stdbool.h>
#include <pthread.h>

#define LOGI(format, ...)                                                      \
  fprintf(stdout, "INF [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(format, ...)                                                      \
  fprintf(stderr, "ERR [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

typedef struct filter_s {
    pthread_mutex_t  lock;
    bool             run;
    AVFilterGraph *  filter_graph;
    AVFilterContext *buffer_src_ctx;
    AVFilterContext *buffer_sink_ctx;
    AVFrame *        filt_frame;
    void (*filter_done_cb)(struct filter_s *ctx, AVFrame *);
} filter_t;

int32_t filter_init(filter_t **filter_ctx, enum AVPixelFormat pix_fmt, int w, int h, void (*filter_done_cb)(filter_t *ctx, AVFrame *frame));

int32_t filter_deinit(filter_t *filter);

int32_t filter_process_frame(filter_t *filter, AVFrame *frame);


#endif //FFMPEG_3000_VIDEO_FILTER_H
