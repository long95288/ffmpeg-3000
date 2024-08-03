#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "libavcodec/avcodec.h"
#include "capture_screen.h"

// 展示录制
static bool g_exit = true;

#define LOGE(format, ...) fprintf(stderr, "ERR [%s:%d]:" format "\n",  __FUNCTION__,__LINE__, ## __VA_ARGS__)

static handle_encode_data_t g_handle_encode_data = NULL;

void screen_capture_set_handle_data(handle_encode_data_t handle) {
    g_handle_encode_data = handle;
}

static void handle_encode_output(uint8_t *buf, int len) {
#if 0
    static FILE *out_file = NULL;
    if (NULL == out_file) {
        out_file = fopen("output.h265", "wb");
        if (out_file == NULL) {
            LOGE("fopen failed\n");
            return;
        }
    }
    // 写入文件
    fwrite(buf, 1, len, out_file);
    fflush(out_file);
#endif
    if (NULL != g_handle_encode_data) {
        g_handle_encode_data(buf, len);
    }
}

// 编码成h265
void encode(AVFrame *frame) {
    static const AVCodec *codec = NULL;
    static AVCodecContext *codec_ctx = NULL;
    static AVFrame *sws_out_frame = NULL;
    static int pts = 0;
    static AVPacket *pkt = NULL;
    // 格式转换器
    static struct SwsContext *sws_ctx = NULL;


    int ret = 0;
    if (NULL == codec) {
        // create
        codec = avcodec_find_encoder(AV_CODEC_ID_H265);
        if (NULL == codec) {
            fprintf(stderr, "Codec '%s' not found\n", avcodec_get_name(AV_CODEC_ID_H265));
            return;
        }
    }

    if (NULL == codec_ctx) {
        codec_ctx = avcodec_alloc_context3(codec);
        if (NULL == codec_ctx) {
            fprintf(stderr, "Could not allocate video codec context\n");
            return;
        }
        // 配置编码器
        codec_ctx->bit_rate = 25 * 1000 * 1000;
        codec_ctx->width = frame->width;
        codec_ctx->height = frame->height;
        codec_ctx->time_base = (AVRational){1, 25};
        codec_ctx->framerate = (AVRational){25, 1};
        codec_ctx->gop_size = 25;
        codec_ctx->max_b_frames = 0;
        codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        // 打开编码器
        ret = avcodec_open2(codec_ctx, codec, NULL);
        if (ret < 0) {
            fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
            avcodec_free_context(&codec_ctx);
            codec_ctx = NULL;
            return;
        }
    }

    if (NULL == pkt) {
        pkt = av_packet_alloc();
        if (NULL == pkt) {
            fprintf(stderr, "av_packet_alloc failed\n");
            return;
        }
    }

    // 转码帧
    if (NULL == sws_out_frame) {
        sws_out_frame = av_frame_alloc();
        if (NULL == sws_out_frame) {
            fprintf(stderr, "av_frame_alloc failed\n");
            return;
        }
        sws_out_frame->format = codec_ctx->pix_fmt;
        sws_out_frame->width = codec_ctx->width;
        sws_out_frame->height = codec_ctx->height;
        printf("sws output frame %dx%d %d\n",
               sws_out_frame->width,
               sws_out_frame->height,
               sws_out_frame->format
        );
        ret = av_frame_get_buffer(sws_out_frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate the video frame data\n");
            av_frame_free(&sws_out_frame);
            sws_out_frame = NULL;
            return;
        }
    }

    // 转码器
    if (NULL == sws_ctx) {
        sws_ctx = sws_getContext(
                // input size
                frame->width,
                frame->height,
                frame->format,

                // output size
                sws_out_frame->width,
                sws_out_frame->height,
                sws_out_frame->format,

                SWS_BILINEAR,
                NULL, NULL, NULL
        );
        if (NULL == sws_ctx) {
            LOGE("sws_getContext failed\n");
            return;
        }
        printf("scale encode input %dx%d[%d], output %dx%d[%d]\n",
                // input
               frame->width,
               frame->height,
               frame->format,
                // output
               sws_out_frame->width,
               sws_out_frame->height,
               sws_out_frame->format
        );
    }


    // 格式转换
    int out_height = sws_scale(sws_ctx,
                               (const uint8_t *const *) frame->data,
                               frame->linesize,
                               0,
                               frame->height,
                               (const uint8_t *const *)sws_out_frame->data,
                               sws_out_frame->linesize
    );
    if (out_height <0) {
        LOGE("sws_scale failed %d\n", out_height);
        return;
    }
    printf("sws_scale out_height %d\n", out_height);
    sws_out_frame->pts = pts;
    pts ++;

    ret = avcodec_send_frame(codec_ctx, sws_out_frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        }else if(ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            return;
        }
        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);

        handle_encode_output(pkt->data, pkt->size);

        av_packet_unref(pkt);
    }
    printf("finish one frame\n");
}


void *th_capture_screen(void *arg) {
    int ret = 0;
    avdevice_register_all();

    AVFormatContext *av_format_context = avformat_alloc_context();
    const AVInputFormat *av_input_format = av_find_input_format("gdigrab");

    // 查看所有的输入设备,linux和windows不一样
    printf("video name %s, long name: %s\n", av_input_format->name, av_input_format->long_name);

    AVDictionary *options = NULL;
    // 设置帧率
    av_dict_set(&options, "framerate", "30", 0);

    // 捕获区域跟随鼠标走,鼠标在中心
//    av_dict_set(&options, "follow_mouse", "centered", 0);
    // 捕获的图像大小,默认是全屏
//    av_dict_set(&options, "video_size", "1280x720", 0);

    ret = avformat_open_input(&av_format_context, "desktop", av_input_format, &options);
    if (ret != 0) {
        printf("avformat_open_input failed\n");
    }

    av_dict_free(&options);

    // 查看流信息,内部会读取一部分的数据获取到流信息
    ret = avformat_find_stream_info(av_format_context, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return ret;
    }

    // 打印流信息
    av_dump_format(av_format_context, 0, NULL, 0);

    // 创建解码器
    AVCodecParameters *codec_parameters = av_format_context->streams[0]->codecpar;
    AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);

    printf("Video Codec: %d x %d\n", codec_parameters->width, codec_parameters->height);

    // 放缩大小
    float scale = 1.0f;

    printf("Codec %s ID %d bit_rate %ld\n", codec->long_name, codec->id, codec_parameters->bit_rate);
    printf("codec name %s\n", codec->name);

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codec_parameters);
    if (0 != avcodec_open2(codec_ctx, codec, NULL)) {
        fprintf(stderr, "avcodec_open2 failed\n");
        return ret;
    }

    // 获取一个帧
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    // 格式转码
#if 0
    AVFrame *result = av_frame_alloc();
    result->format = AV_PIX_FMT_RGB24;
    int render_w = codec_parameters->width * scale;
    int render_h = codec_parameters->height *scale;
    result->width = render_w;
    result->height = render_h;
    av_frame_get_buffer(result, 1);
#endif

#if 0
    // 缩放
    struct SwsContext *sws_ctx = NULL;
    sws_ctx = sws_getContext(
            // input size
            codec_parameters->width, codec_parameters->height, codec_parameters->format,
            // output size
            result->width, result->height, result->format,
            // 放缩算法
            SWS_BILINEAR,
            NULL, NULL, NULL);
#endif
    while (!g_exit && av_read_frame(av_format_context, packet) >= 0) {
        // 解码
        ret = avcodec_send_packet(codec_ctx, packet);
        if (ret < 0 || ret == AVERROR(EAGAIN)) {
            fprintf(stderr, "avcodec_send_packet not ready\n");
            break;
        }
        // 查看是否解码结束
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret < 0) {
            fprintf(stderr, "avcodec_receive_frame failed\n");
            break;
        }
        printf("Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d] format %d"
               "\n",
               av_get_picture_type_char(frame->pict_type),
               codec_ctx->frame_number,
               frame->pts,
               frame->pkt_dts,
               frame->key_frame,
               frame->coded_picture_number,
               frame->display_picture_number,
               frame->format
        );
#if 0
        // 缩放
        sws_scale(sws_ctx,
                  (const uint8_t *const *) frame->data,
                  frame->linesize,
                  0,
                  frame->height,
                  result->data,
                  result->linesize);
        printf("scale play output %dx%d=>%dx%d\n", frame->width, frame->height, result->width, result->height);
#endif
        // 发送到编码器
        encode(frame);

        // 清理内存,避免内存泄露
        av_frame_unref(frame);
        av_packet_unref(packet);
    }

    // 释放资源
#if 0
    sws_freeContext(sws_ctx);
    av_frame_free(&result);
#endif
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);

    avformat_close_input(&av_format_context);
    avformat_free_context(av_format_context);

    return NULL;
}

static pthread_t capture_hr;

int start_capture() {
    g_exit = false;
    pthread_create(&capture_hr, NULL, th_capture_screen, NULL);
    return 0;
}

int stop_capture() {
    g_exit = true;
    pthread_join(capture_hr, NULL);
    return 0;
}
