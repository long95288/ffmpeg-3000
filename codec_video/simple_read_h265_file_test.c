#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"

#define LOGI(format, ...)                                                      \
  fprintf(stdout, "INF [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(format, ...)                                                      \
  fprintf(stderr, "ERR [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

int main(int argc, char *argv[]) {
    int ret = 0;
//    char *input_file = argv[1];
    char *input_file = "./output.h265";
    AVFormatContext *ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, input_file, NULL, NULL))) {
        LOGE("open %s failed %s", input_file, strerror(errno));
        return ret;
    }
    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Could not find stream information\n");
        avformat_close_input(&ifmt_ctx);
        return ret;
    }

    // 打印流信息
    av_dump_format(ifmt_ctx, 0, NULL, 0);
    AVCodecParameters *codecpar = ifmt_ctx->streams[0]->codecpar;
    printf("code type  :%s\n", codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? "video" : "other");
    printf("codec      :%s\n", avcodec_get_name(codecpar->codec_id));
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if ((ret = avcodec_open2(codec_ctx, codec, NULL))) {
        LOGE("avcodec_open2 failed\n");
        avformat_close_input(&ifmt_ctx);
        return ret;
    }

    // nalu包
    AVPacket *packet = NULL;
    packet = av_packet_alloc();

    // yuv帧
    AVFrame *frame = av_frame_alloc();
    while (av_read_frame(ifmt_ctx, packet) >= 0) {
        // 获取到nalu包: 一个packet为一个nalu包, 起始码为00000001
        ret = avcodec_send_packet(codec_ctx, packet);
        if (ret < 0 || ret == AVERROR(EAGAIN)) {
            LOGE("avcodec_send_packet not ready\n");
            break;
        }

        // 获取yuv帧
        while (!avcodec_receive_frame(codec_ctx, frame)) {
            printf("frame fmt: %c, %s\n",
                   av_get_picture_type_char(frame->pict_type),
                   av_get_pix_fmt_name(frame->format)
            );
            av_packet_unref(packet);
            av_frame_unref(frame);
        }
    }
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 0;
}