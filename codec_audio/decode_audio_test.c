//
// Created by root on 2024/8/4.
//

#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"

// test read audio file and got stream info
int test_decode_mp3_file() {
    int ret = 0;
    // need var
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;

    //
    avdevice_register_all();
    //
    char *input_file = "./test.mp3";
    if((ret = avformat_open_input(&ifmt_ctx, input_file, NULL, NULL))) {
        fprintf(stderr, "open %s failed %s", input_file, strerror(errno));
        return ret;
    }
    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        avformat_close_input(&ifmt_ctx);
        return ret;
    }
    // 打印流信息
    av_dump_format(ifmt_ctx, 0, NULL, 0);
    AVCodecParameters *codecpar = ifmt_ctx->streams[0]->codecpar;
    printf("code type  :%s\n", codecpar->codec_type == AVMEDIA_TYPE_AUDIO ? "audio" : "other");
    printf("codec      :%s\n", avcodec_get_name(codecpar->codec_id));
    printf("sample     :%s\n", av_get_sample_fmt_name(codecpar->format));
    printf("channel    :%d\n", codecpar->ch_layout.nb_channels);
    printf("sample_rate:%d\n", codecpar->sample_rate);

    // decode
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(avcodec_find_decoder(codecpar->codec_id));
    printf("Codec %s ID %d bit_rate %ld\n", codec->long_name, codec->id, codecpar->bit_rate);
    printf("codec name %s\n", codec->name);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (0 != (ret = avcodec_open2(codec_ctx, codec, NULL))) {
        fprintf(stderr, "avcodec_open2 failed\n");
        return ret;
    }

    FILE *output_fp = fopen("out.pcm", "wb");
    AVPacket *packet = NULL;
    packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while(av_read_frame(ifmt_ctx, packet) >= 0) {
        // 读出来的是MP3的packet
        ret = avcodec_send_packet(codec_ctx, packet);
        if (ret < 0 || ret == AVERROR(EAGAIN)) {
            fprintf(stderr, "avcodec_send_packet not ready\n");
            break;
        }
        // FLTP float32 pcm
        //
        while (!avcodec_receive_frame(codec_ctx, frame)) {
            printf("frame fmt: %s\n", av_get_sample_fmt_name(frame->format));
            int nb = av_get_bytes_per_sample(codec_ctx->sample_fmt);
            printf("nb %d, %d, %d\n", nb, frame->nb_samples, frame->linesize[0]);
            // save left channel
            // note: not normal s16le pcm audio, need open use float32 pcm
            fwrite((uint8_t *)frame->data[0], 1, frame->nb_samples * nb, output_fp);
            fflush(output_fp);
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

int main(int argc,char *argv[]) {
    test_decode_mp3_file();
}

