//
// Created by root on 2025/2/7.
//

#include "video_filter.h"
//////////////////////////
///    过滤器,锐化等     ///
//////////////////////////


int32_t filter_init(filter_t **filter_ctx, enum AVPixelFormat pix_fmt, int w, int h, void (*filter_done_cb)(filter_t *ctx, AVFrame *frame)) {
    char args[512] = { 0 };
    int  ret = 0;

    AVFilterGraph *  filter_graph;
    AVFilterContext *buffer_src_ctx;
    AVFilterContext *buffer_sink_ctx;

    const AVFilter *buffer_src = avfilter_get_by_name("buffer");
    const AVFilter *buffer_sink = avfilter_get_by_name("buffersink");
    AVFilterInOut * outputs = avfilter_inout_alloc();
    AVFilterInOut * inputs = avfilter_inout_alloc();

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        LOGE("create resource failed, no memory");
        goto end;
    }
    //// 输入过滤器
    // 需要注意每个参数的含义是什么?
    AVRational timebase = { 1, 1000 };
    sprintf(args, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d", w, h, pix_fmt, timebase.num, timebase.den);

    ret = avfilter_graph_create_filter(&buffer_src_ctx, buffer_src, "in", args, NULL, filter_graph);
    if (ret < 0) {
        LOGE("avfilter_graph_create_filter in failed %d\n", ret);
        goto end;
    }

    // 输出过滤器
    ret = avfilter_graph_create_filter(&buffer_sink_ctx, buffer_sink, "out", NULL, NULL, filter_graph);
    if (ret < 0) {
        LOGE("avfilter_graph_create_filter in failed %d\n", ret);
        goto end;
    }
    enum AVPixelFormat pix_fmts[] = { pix_fmt, AV_PIX_FMT_NONE };

    av_opt_set_int_list(buffer_sink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    // 这个output对应的含义是什么?
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffer_src_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    // inputs
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffer_sink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    // 过滤器类型:
    // 锐化
    char *filter_descr = "unsharp=luma_msize_x=7:luma_msize_y=7:luma_amount=2.5";
    // 将噪
    //    char *filter_descr = "hqdn3d";
    ret = avfilter_graph_parse_ptr(filter_graph, filter_descr, &inputs, &outputs, NULL);
    if (ret < 0) {
        LOGE("avfilter_graph_parse_ptr failed\n");
        goto end;
    }
    //
    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0) {
        LOGE("avfilter_graph_config failed\n");
        goto end;
    }

    filter_t *filter = (filter_t *)calloc(1, sizeof(filter_t));
    pthread_mutex_init(&filter->lock, NULL);
    filter->run = true;
    filter->filter_graph = filter_graph;
    filter->buffer_sink_ctx = buffer_sink_ctx;
    filter->buffer_src_ctx = buffer_src_ctx;
    filter->filt_frame = av_frame_alloc();
    filter->filter_done_cb = filter_done_cb;

    *filter_ctx = filter;
    end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return 0;
}

int32_t filter_deinit(filter_t *filter) {
    pthread_mutex_lock(&filter->lock);
    filter->run = false;
    avfilter_graph_free(&filter->filter_graph);
    filter->filter_graph = NULL;
    av_frame_free(&filter->filt_frame);
    filter->filt_frame = NULL;
    filter->filter_done_cb = NULL;
    pthread_mutex_unlock(&filter->lock);

    pthread_mutex_destroy(&filter->lock);

    free(filter);
    return 0;
}

int32_t filter_process_frame(filter_t *filter, AVFrame *frame) {
    int ret = 0;
    // 经过过滤器
    if (filter == NULL) {
        return -1;
    }
    pthread_mutex_lock(&filter->lock);
    if (filter != NULL && filter->run) {
        if ((ret = av_buffersrc_add_frame_flags(filter->buffer_src_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0) {
        } else {
            while (filter->run) {
                ret = av_buffersink_get_frame(filter->buffer_sink_ctx, filter->filt_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    break;
                }
                if (NULL != filter->filter_done_cb) {
                    filter->filter_done_cb(filter, filter->filt_frame);
                }
                av_frame_unref(filter->filt_frame);
            }
        }
    }
    pthread_mutex_unlock(&filter->lock);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////
