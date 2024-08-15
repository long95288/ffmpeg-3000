#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "libavcodec/avcodec.h"

#define LOGI(format, ...)                                                      \
  fprintf(stdout, "INF [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(format, ...)                                                      \
  fprintf(stderr, "ERR [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

//
#define INPUT_BUF_SIZE 4096

int main(int argc,char *argv[]) {
    int ret = 0;
    //    char *input_file = argv[1];
    char *input_file = "./test_audio.g711a";
    FILE *input_fp = fopen(input_file, "rb");
    if (NULL == input_fp) {
        LOGE("open file %s failed %s\n", input_file, strerror(errno));
        return -1;
    }
    // 申请资源
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_PCM_ALAW);
   
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    // known from 
    AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_MONO;
    codec_ctx->ch_layout = ch_layout;
    codec_ctx->sample_rate = 16000;
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;

    if ((ret = avcodec_open2(codec_ctx, codec, NULL))) {
        LOGE("avcodec_open2 failed\n");
        return ret;
    }

    // 缓冲区
    uint8_t input_buf[INPUT_BUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    size_t read_size = 0;
    uint8_t *read_data = NULL;
    
    AVPacket packet = {0};
    
    AVFrame *frame = av_frame_alloc();
    
    FILE *output_fp = fopen("output.pcm", "wb");
    
    int len = 0;
    while (1) {
        read_size = fread(input_buf, 1,INPUT_BUF_SIZE, input_fp);
        if (read_size == 0) break;
        read_data = input_buf;
        // push buf
        while (read_size > 0) {
            uint8_t *output_buf = NULL;
            int output_buf_size;
            
            len = read_size;
            output_buf_size = read_size;
            output_buf = read_data;

            // remain buf
            read_size -= len;
            read_data += len;
            if (output_buf_size == 0) continue;

            packet.data = output_buf;
            packet.size = output_buf_size;
            
            ret = avcodec_send_packet(codec_ctx, &packet);
            if (ret < 0 || ret == AVERROR(EAGAIN)) {
                LOGE("avcodec_send_packet not ready\n");
                break;
            }
            // receive decode frame
            while (!avcodec_receive_frame(codec_ctx, frame)) {
                printf("frame fmt %s %d %d %d %d\n",
                       av_get_sample_fmt_name(frame->format),
                       frame->sample_rate,
                       frame->nb_samples,
                       frame->ch_layout.nb_channels,
                       av_get_bytes_per_sample(frame->format)
                );
                // save 
                int nb =  av_get_bytes_per_sample(frame->format);
                fwrite((uint8_t *)frame->data[0], 1,  frame->nb_samples * nb, output_fp);
                av_frame_unref(frame);
            }
        }
    }
    fflush(output_fp);

    av_frame_free(&frame);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    fclose(input_fp);
    fclose(output_fp);
    return 0;
}
