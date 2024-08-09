#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
#include "capture_screen.h"

#define LOGE(format, ...)                                             \
  fprintf(stderr, "ERR [%s:%d]:" format "\n", __FUNCTION__, __LINE__, \
          ##__VA_ARGS__)

// capture ctx
typedef struct {
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
    AVPacket *packet;
    AVFrame *frame;
    
    pthread_t hr;
    bool run;
    video_frame_callback_t func;
} capture_ctx_t;

void *th_read_capture(void *arg) {
    int ret = 0;
    capture_ctx_t *ctx = arg;
    AVFormatContext *fmt_ctx = ctx->fmt_ctx;
    AVPacket *packet = ctx->packet;
    AVCodecContext *codec_ctx = ctx->codec_ctx;
    AVFrame *frame = ctx->frame; 
    while (ctx->run && av_read_frame(fmt_ctx, packet) >= 0) {
        // 解码
        ret = avcodec_send_packet(codec_ctx, packet);
        if (ret < 0 || ret == AVERROR(EAGAIN)) {
            LOGE("avcodec_send_packet not ready\n");
            continue;
        }
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret < 0) {
            fprintf(stderr, "avcodec_receive_frame failed\n");
            sleep(2);
            continue;
        }
#if 0
        printf(
                "Frame %c (%d) pts %ld dts %ld key_frame %d [coded_picture_number %d, "
                "display_picture_number %d] format [%d]%s"
                "\n",
                av_get_picture_type_char(frame->pict_type), codec_ctx->frame_number,
                frame->pts, frame->pkt_dts, frame->key_frame,
                frame->coded_picture_number, frame->display_picture_number,
                frame->format, av_get_pix_fmt_name(frame->format));
#endif
        if (NULL != ctx->func) {
            ctx->func(ctx, frame);
        }
        // reset ptr fre
        av_frame_unref(frame);
        av_packet_unref(packet); 
    }
    return NULL;
}

// capture screen picture
void *video_capture_start(video_frame_callback_t func) {
    int ret = 0;
    avdevice_register_all();

    AVFormatContext *fmt_ctx = NULL;
    const AVInputFormat *ifmt = av_find_input_format("gdigrab");
    
    // 查看所有的输入设备,linux和windows不一样
    printf("video name %s, long name: %s\n", ifmt->name, ifmt->long_name);

    AVDictionary *options = NULL;
    // 设置帧率
    av_dict_set(&options, "framerate", "30", 0);
    // 捕获区域跟随鼠标走,鼠标在中心
//    av_dict_set(&options, "follow_mouse", "centered", 0);
    // 捕获的图像大小,默认是全屏
    av_dict_set(&options, "video_size", "1280x720", 0);
    
    if ((ret =avformat_open_input(&fmt_ctx, "desktop", ifmt, &options)) != 0) {
        LOGE("avformat_open_input failed %d\n", ret);
        avformat_free_context(fmt_ctx);
        return NULL;
    }
    av_dict_free(&options);
    // 查看流信息,内部会读取一部分的数据获取到流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Could not find stream information\n");
        avformat_close_input(&fmt_ctx);
        avformat_free_context(fmt_ctx);
        return NULL;
    }
    // 打印流信息
    av_dump_format(fmt_ctx, 0, NULL, 0);
    // 创建解码器
    AVCodecParameters *codec_para = fmt_ctx->streams[0]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_para->codec_id);
    printf("Video Codec: %d x %d\n", codec_para->width, codec_para->height);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codec_para);
    if (0 != (ret = avcodec_open2(codec_ctx, codec, NULL))) {
        LOGE("avcodec_open2 failed %d\n", ret);
        avformat_close_input(&fmt_ctx);
        avformat_free_context(fmt_ctx);
        return NULL;
    }
    // 获取一个帧
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    capture_ctx_t *cap_ctx = (capture_ctx_t *) calloc(1, sizeof(capture_ctx_t));
    cap_ctx->fmt_ctx = fmt_ctx;
    cap_ctx->codec_ctx = codec_ctx;
    cap_ctx->packet = packet;
    cap_ctx->frame = frame;
    cap_ctx->func = func;
    cap_ctx->run = true;
    pthread_create(&cap_ctx->hr, NULL, th_read_capture, cap_ctx);
    return cap_ctx;
}

int video_capture_stop(void *ctx) {
    capture_ctx_t *cap_ctx = ctx;
    cap_ctx->run = false;
    pthread_join(cap_ctx->hr, NULL);
    avformat_close_input(&cap_ctx->fmt_ctx);
    av_packet_free(&cap_ctx->packet);
    av_frame_free(&cap_ctx->frame);
    return 0;
}

