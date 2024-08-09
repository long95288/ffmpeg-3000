#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"

int handle_pcm_raw_data(uint8_t *data, size_t data_size) {
    static FILE *output_file = NULL;
    if (NULL == output_file) {
        output_file = fopen("./output_audio.pcm", "wb");
    }
    printf("write to file %ld bytes \n", data_size);
    fwrite(data, 1, data_size, output_file);
    fflush(output_file);
    return 0;
}

// 解码,生成ftlp的pcm流之后再进行重采样
int handle_pcm_frame(AVFrame *frame) {
    int                 ret = 0;
    AVChannelLayout     src_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    int                 src_rate = 0;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_NONE;

    // dst config
    AVChannelLayout     dst_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    int                 dst_rate = 16000;  // 16khz
    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;

    struct SwrContext *swr_ctx = NULL;
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        return -1;
    }
    // 两个通道都采样->转成1通道,注意跟只重采样左声道是不一样的。
    src_ch_layout = frame->ch_layout;
    src_rate = frame->sample_rate;
    src_sample_fmt = frame->format;

    /* set options */
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

    av_opt_set_chlayout(swr_ctx, "out_chlayout", &dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        swr_free(&swr_ctx);
        return ret;
    }

    // 输入缓冲区
    // 两个通道都采样->转成1通道,注意跟只重采样左声道是不一样的。
    uint8_t **src_data = frame->data;
    int       src_nb_samples = frame->nb_samples;
    int       src_linesize = frame->linesize[0];
    int       src_nb_channels = frame->ch_layout.nb_channels;

    // 输出缓冲区
    uint8_t **dst_data = NULL;
    int       dst_nb_samples = 0;
    int       dst_linesize = 0;
    int       dst_nb_channels = dst_ch_layout.nb_channels;
    // 采样算法
    dst_nb_samples = av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
    ret = av_samples_alloc_array_and_samples(
        /*output*/ &dst_data,
        /*output*/ &dst_linesize,
        /*input*/ dst_nb_channels,
        /*input*/ dst_nb_samples,
        dst_sample_fmt,
        0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        swr_free(&swr_ctx);
        return ret;
    }

    printf("############################\n");
    printf("input : \n");
    printf("src_nb_channels: %d\n", src_nb_channels);
    printf("src_nb_samples : %d\n", src_nb_samples);
    printf("src_linesize   : %d\n", src_linesize);

    printf("############################\n");
    printf("############################\n");
    printf("output :\n");
    printf("dst_nb_channels: %d\n", dst_nb_channels);
    printf("dst_nb_samples : %d\n", dst_nb_samples);
    printf("dst_linesize   : %d\n", dst_linesize);
    printf("############################\n");
    // swr_convert 需要多调几次,只调用一次的声音会变短的,有杂音。
    while ((ret = swr_convert(
                swr_ctx,
                /**/ dst_data,
                /**/ dst_nb_samples,
                /*原数据*/ (const uint8_t **)src_data,
                /*原数据个数*/ src_nb_samples))
           > 0) {
        int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, ret, dst_sample_fmt, 1);
        if (dst_bufsize < 0) {
            fprintf(stderr, "Could not get sample buffer size\n");
            goto end;
        }
        // write to file
        handle_pcm_raw_data(dst_data[0], dst_bufsize);
        src_data = NULL;
        src_nb_samples = 0;
    }
end:
    // resample success
    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);
    swr_free(&swr_ctx);
    return 0;
}

int main(int argc, char *argv[]) {
    // read diff file and resample to 16bit 16khz 1ch pcm
    // src config, will override from file

    // 读取音频文件,获取到配置信息
    int              ret = 0;
    char *           input_file = argv[1];
    AVFormatContext *ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, input_file, NULL, NULL))) {
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
    printf("format     :%s\n", av_get_sample_fmt_name(codecpar->format));
    printf("channel    :%d\n", codecpar->ch_layout.nb_channels);
    printf("sample_rate:%d\n", codecpar->sample_rate);

    // decode
    const AVCodec * codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(avcodec_find_decoder(codecpar->codec_id));
    printf("Codec %s ID %d bit_rate %ld\n", codec->long_name, codec->id, codecpar->bit_rate);
    printf("codec name %s\n", codec->name);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (0 != (ret = avcodec_open2(codec_ctx, codec, NULL))) {
        fprintf(stderr, "avcodec_open2 failed\n");
        return ret;
    }
    AVPacket *packet = NULL;
    packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    FILE *   output_fp = fopen("out.pcm", "wb");
    while (av_read_frame(ifmt_ctx, packet) >= 0) {
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
            handle_pcm_frame(frame);
            //
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
    fclose(output_fp);
    return 0;
}
