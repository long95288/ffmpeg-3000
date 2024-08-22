//
// Created by root on 2024/8/22.
//
// target read 16le 16000 16biy pcm and resample to 44100 16bit pcm

#include "libswresample/swresample.h"
#include "libavformat/avformat.h"
#include "libavutil/opt.h"

int main(int argc,char *argv[]) {
    int ret = 0;
    // src
    AVChannelLayout     src_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    int                 src_rate = 16000;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_S16;
    
    // dst
    AVChannelLayout     dst_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    int                 dst_rate = 44100;
    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_FLTP;

    struct SwrContext *swr_ctx = NULL;

    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        return -1;
    }

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
    uint8_t **src_data = NULL;
    int       src_nb_samples = 1024; // 1024个样本
    int       src_linesize = 0;
    // 申请内存
    ret = av_samples_alloc_array_and_samples(
            &src_data, &src_linesize,
            src_ch_layout.nb_channels,
            src_nb_samples,
            src_sample_fmt, 0
            );
    if (ret < 0) {
        fprintf(stderr, "Could not allocate src samples\n");
        swr_free(&swr_ctx);
        return ret;
    }
    
    // input 缓冲区
    printf("input : line_size %d, dst_nb_samples %d\n", src_linesize, src_nb_samples);

    // 输出缓冲区
    uint8_t **dst_data = NULL;
    int       dst_nb_samples = 1024; // 输出样本
    int       dst_linesize = 0;
    int       dst_nb_channels = dst_ch_layout.nb_channels;
    // 申请内存
    ret = av_samples_alloc_array_and_samples(
            &dst_data, &dst_linesize,
            dst_ch_layout.nb_channels,
            dst_nb_samples,
            dst_sample_fmt, 0
            );
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        swr_free(&swr_ctx);
        
        if (src_data)
            av_freep(&src_data[0]);
        av_freep(&src_data);
        return ret;
    }
    
    printf("output : line_size %d, dst_nb_samples %d\n", dst_linesize, dst_nb_samples);
    FILE *input_fp = fopen("./input_16k.pcm", "rb");
    FILE *output_fp = fopen("./output_44100.pcm", "wb");
    int src_nb = av_get_bytes_per_sample(src_sample_fmt);
    int total_read_size = 0;
    int total_write_size = 0;
    while ((ret = fread(src_data[0], 1, src_nb * src_nb_samples, input_fp)) > 0){
        printf("sws input size: %d\n", ret);
        total_read_size += ret;
        uint8_t **_tmp_src_data = src_data;
        int _tmp_src_nb_samples = ret / src_nb;
        // 需要多读很多次才能读完
        while ((ret = swr_convert(swr_ctx, dst_data, dst_nb_samples,
                                (const uint8_t **)_tmp_src_data,
                                  _tmp_src_nb_samples)) > 0) {
            if (ret < 0) {
                fprintf(stderr, "swr_convert failed");
                break;
            }
            int dst_buf_size = av_samples_get_buffer_size(
                    &dst_linesize, dst_nb_channels,
                    ret, dst_sample_fmt, 1
            );
            printf("sws output size: %d\n", dst_buf_size);
            fwrite(dst_data[0], 1, dst_buf_size, output_fp);
            total_write_size += dst_buf_size;
            _tmp_src_data = NULL;
            _tmp_src_nb_samples = 0;
        }
    }
    printf("read input file size %d bytes\n", total_read_size);
    printf("write output file size %d bytes\n", total_write_size);
    fflush(output_fp);
    
    // free resource
    swr_free(&swr_ctx);
    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);
    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);
    fclose(input_fp);
    fclose(output_fp);
    return 0;
}
