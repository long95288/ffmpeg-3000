//
// Created by lqx on 2024/8/21.
// read FLTP 44100 audio file and encode to aac & mp3
// fltp_44100.pcm
#include <libavutil/opt.h>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/adts_parser.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"

// adts format
//
static void get_adts_header(AVCodecContext *ctx, uint8_t *adts_header, int aac_length)
{
    uint8_t freq_idx = 0;
    switch (ctx->sample_rate) {
        case 96000: freq_idx = 0; break;
        case 88200: freq_idx = 1; break;
        case 64000: freq_idx = 2; break;
        case 48000: freq_idx = 3; break;
        case 44100: freq_idx = 4; break;
        case 32000: freq_idx = 5; break;
        case 24000: freq_idx = 6; break;
        case 22050: freq_idx = 7; break;
        case 16000: freq_idx = 8; break;
        case 12000: freq_idx = 9; break;
        case 11025: freq_idx = 10; break;
        case 8000: freq_idx = 11; break;
        case 7350: freq_idx = 12; break;
        default: freq_idx = 4; break;
    }
    uint8_t chan_cfg = ctx->ch_layout.nb_channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((ctx->profile) << 6) + (freq_idx << 2) + (chan_cfg >> 2);
    adts_header[3] = (((chan_cfg & 3) << 6) + (frame_length >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
}


static void encode_aac(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt,
                       FILE *output)
{
    int ret;

    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }
        // aac adts
        uint8_t aac_header[7] = {0};
        get_adts_header(ctx, aac_header, pkt->size);
        fwrite(aac_header, 1, sizeof(aac_header), output);
        fwrite(pkt->data, 1, pkt->size, output);
        av_packet_unref(pkt);
    }
}

int test_encode_acc() {
    int ret = 0;
    const char *input_file = "./fltp_44100.pcm";
    const char *output_file = "./fltp_44100.aac";

    AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_MONO;
    enum AVCodecID codec_id = AV_CODEC_ID_AAC;

    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx= NULL;

    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;

    // 编码器参数
    AVCodecParameters *codec_param = avcodec_parameters_alloc();
    codec_param->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_param->codec_id = codec_id;
    codec_param->sample_rate=44100;
    codec_param->bit_rate = 128000;
    codec_param->format = AV_SAMPLE_FMT_FLTP;
    codec_param->ch_layout = ch_layout;
    // profile for aac
    codec_param->profile = FF_PROFILE_AAC_LOW;

    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }
    // 复制参数
    avcodec_parameters_to_context(codec_ctx, codec_param);
    avcodec_parameters_free(&codec_param);

    /* open it */
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        exit(1);
    }
    /* frame containing input raw audio */
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        exit(1);
    }

    frame->nb_samples = codec_ctx->frame_size;
    frame->format = codec_ctx->sample_fmt;
    frame->ch_layout = codec_ctx->ch_layout;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        exit(1);
    }
    ret = av_frame_make_writable(frame);
    if (ret < 0){
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }

    FILE *input_fp = fopen(input_file, "rb");
    FILE *output_fp = fopen(output_file, "wb");
    int pts = 0;
    int nb = av_get_bytes_per_sample(frame->format);
    while ((ret = fread(frame->data[0], 1, nb * frame->nb_samples, input_fp)) > 0) {
        frame->nb_samples = ret / nb;
        frame->pts = (++pts);
        // send frame
        encode_aac(codec_ctx, frame, pkt, output_fp);
        ret = av_frame_make_writable(frame);
        if (ret < 0){
            fprintf(stderr, "Error av_frame_make_writable\n");
            exit(1);
        }
    }
    // handle receive last frame
    encode_aac(codec_ctx, NULL, pkt, output_fp);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);

    fflush(output_fp);

    fclose(input_fp);
    fclose(output_fp);
    return 0;
}

#define LOGE(format, ...) fprintf(stderr, "ERR [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) fprintf(stdout, "INF [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

static void encode_mp3(AVCodecContext *ctx,AVFormatContext *fmt_ctx, AVFrame *frame, AVPacket *pkt)
{
    int ret;

    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }
        av_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }
}

// use newest api to encode pcm to mp3
int test_encode_mp3() {
    int ret = 0;
    const char *input_file = "./fltp_44100.pcm";
    const char *output_file = "./fltp_44100.mp3";
    AVFormatContext *fmt_ctx = NULL;

    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;


    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;

    if ((ret = avformat_alloc_output_context2(
            &fmt_ctx, NULL, NULL, output_file
    ))){
        LOGE("avformat_alloc_output_context2 failed %d\n", ret);
        return -1;
    }
    const AVOutputFormat *ofmt =  fmt_ctx->oformat;

    LOGI("ofmt %s", ofmt->name);

    if ((ret = avio_open(&fmt_ctx->pb, output_file, AVIO_FLAG_READ_WRITE)) < 0) {
        LOGE("avio_open failed ret %d\n", ret);
        return -1;
    }
    // 新增一个流
    AVStream *ostream = avformat_new_stream(fmt_ctx, NULL);
    if (!ostream) {
        LOGE("avformat_new_stream failed\n");
        return -1;
    }
    // 编码器参数
    AVCodecParameters *codec_param = fmt_ctx->streams[ostream->index]->codecpar;
    codec_param->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_param->codec_id = ofmt->audio_codec;
    codec_param->sample_rate=44100;
    codec_param->bit_rate = 128000;
    codec_param->format = AV_SAMPLE_FMT_FLTP;
    AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_MONO;
    codec_param->ch_layout = ch_layout;

    // 初始化编码器
    codec = avcodec_find_encoder(ofmt->audio_codec);
    if (NULL == codec) {
        LOGE("avcodec_find_encoder failed\n");
        return -1;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == NULL) {
        LOGE("avcodec_alloc_context3 failed\n");
        return -1;
    }
    // 复制参数
    avcodec_parameters_to_context(codec_ctx, codec_param);

    // 打开编码器
    if ((ret = avcodec_open2(codec_ctx, codec, NULL))< 0) {
        LOGE("avcodec_open2 failed %d", ret);
        return ret;
    }

    // 查看编码信息
    av_dump_format(fmt_ctx, ostream->index, output_file, 1);

    // 准备帧数据
    frame = av_frame_alloc();
    pkt = av_packet_alloc();

    frame->format = codec_ctx->sample_fmt;
    frame->nb_samples = codec_ctx->frame_size;
    frame->ch_layout = codec_ctx->ch_layout;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        LOGE("Could not allocate audio data buffers %d\n", ret);
        return ret;
    }
    ret = av_frame_make_writable(frame);

    // 写入帧头
    ret = avformat_write_header(fmt_ctx, NULL);

    // 写
    FILE *input_fp = fopen(input_file, "rb");
    int pts = 0;
    int nb = av_get_bytes_per_sample(frame->format);
    while ((ret = fread(frame->data[0], 1, nb * frame->nb_samples, input_fp)) > 0) {
        frame->nb_samples = ret / nb;
        frame->pts = (++pts);
        encode_mp3(codec_ctx, fmt_ctx, frame, pkt);
    }
    // handle receive last frame
    encode_mp3(codec_ctx, fmt_ctx, NULL, pkt);

    // 写入容器尾
    av_write_trailer(fmt_ctx);

    avcodec_close(codec_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avio_close(fmt_ctx->pb);
    avformat_free_context(fmt_ctx);

    return ret;
}

static void encode_mp3_v2(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt,
                          FILE *output)
{
    int ret;

    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }
        fwrite(pkt->data, 1, pkt->size, output);
        av_packet_unref(pkt);
    }
}


int test_encode_mp3_v2() {
    int ret = 0;
    const char *input_file = "./fltp_44100.pcm";
    const char *output_file = "./fltp_44100.mp3";

    AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_MONO;
    enum AVCodecID codec_id = AV_CODEC_ID_MP3;

    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;

    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;

    // 编码器参数
    AVCodecParameters *codec_param = avcodec_parameters_alloc();
    codec_param->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_param->codec_id = codec_id;
    codec_param->sample_rate=44100;
    codec_param->bit_rate = 128000;
    codec_param->format = AV_SAMPLE_FMT_FLTP;
    codec_param->ch_layout = ch_layout;

    // 初始化编码器
    codec = avcodec_find_encoder(codec_id);
    if (NULL == codec) {
        LOGE("avcodec_find_encoder failed\n");
        return -1;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == NULL) {
        LOGE("avcodec_alloc_context3 failed\n");
        return -1;
    }
    // 复制参数
    avcodec_parameters_to_context(codec_ctx, codec_param);
    avcodec_parameters_free(&codec_param);

    // 打开编码器
    if ((ret = avcodec_open2(codec_ctx, codec, NULL))< 0) {
        LOGE("avcodec_open2 failed %d", ret);
        return ret;
    }

    // 准备帧数据
    frame = av_frame_alloc();
    pkt = av_packet_alloc();

    frame->format = codec_ctx->sample_fmt;
    frame->nb_samples = codec_ctx->frame_size;
    frame->ch_layout = codec_ctx->ch_layout;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        LOGE("Could not allocate audio data buffers %d\n", ret);
        return ret;
    }
    ret = av_frame_make_writable(frame);
    if (ret < 0){
        LOGE("Error sending the frame to the encoder %d\n", ret);
        return ret;
    }

    FILE *input_fp = fopen(input_file, "rb");
    FILE *output_fp = fopen(output_file, "wb");
    int pts = 0;
    int nb = av_get_bytes_per_sample(frame->format);
    while ((ret = fread(frame->data[0], 1, nb * frame->nb_samples, input_fp)) > 0) {
        frame->nb_samples = ret / nb;
        frame->pts = (++pts);
        encode_mp3_v2(codec_ctx, frame, pkt, output_fp);
        ret = av_frame_make_writable(frame);
        if (ret < 0){
            LOGE("Error sending the frame to the encoder %d\n", ret);
            return ret;
        }
    }

    // handle receive last frame
    encode_mp3_v2(codec_ctx, NULL, pkt, output_fp);

    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);

    av_frame_free(&frame);
    av_packet_free(&pkt);

    fflush(output_fp);

    fclose(input_fp);
    fclose(output_fp);
    return ret;
}

int main(int argc,char *argv[]) {
    test_encode_acc();
//    test_encode_mp3();
    test_encode_mp3_v2();

    return 0;
}

