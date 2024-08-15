#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"

#define LOGI(format, ...)                                                      \
  fprintf(stdout, "INF [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(format, ...)                                                      \
  fprintf(stderr, "ERR [%s:%d]:" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

//
#define INPUT_BUF_SIZE 4096

int main(int argc,char *argv[]) {
    int ret = 0;
    //    char *input_file = argv[1];
    char *input_file = "./output.h265";
    FILE *input_fp = fopen(input_file, "rb");
    if (NULL == input_fp) {
        LOGE("open file %s failed %s\n", input_file, strerror(errno));
        return -1;
    }
    // 申请资源
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H265);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if ((ret = avcodec_open2(codec_ctx, codec, NULL))) {
        LOGE("avcodec_open2 failed\n");
        return ret;
    }
    
    AVCodecParserContext *parser_ctx = av_parser_init(AV_CODEC_ID_H265);
    
    // 缓冲区
    uint8_t input_buf[INPUT_BUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    size_t read_size = 0;
    uint8_t *read_data = NULL;
    
    AVPacket packet = {0};
    
    AVFrame *frame = av_frame_alloc();
    
    int len = 0;
    while (1) {
        read_size = fread(input_buf, 1,INPUT_BUF_SIZE, input_fp);
        if (read_size == 0) break;
        read_data = input_buf;
        // push buf
        while (read_size > 0) {
            uint8_t *output_buf = NULL;
            int output_buf_size;
            len = av_parser_parse2(parser_ctx, codec_ctx, 
                                   &output_buf, &output_buf_size,
                                   read_data, read_size,
                                   AV_NOPTS_VALUE,
                                   AV_NOPTS_VALUE, AV_NOPTS_VALUE
                                   );
            // remain buf
            read_size -= len;
            read_data += len;
            if (output_buf_size == 0) continue;

//            LOGI("got one nalu packet\n");
            packet.data = output_buf;
            packet.size = output_buf_size;
            
            ret = avcodec_send_packet(codec_ctx, &packet);
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
                av_frame_unref(frame);
            }
        }
    }

    av_frame_free(&frame);
    av_parser_close(parser_ctx);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    fclose(input_fp);
    return 0;
}
