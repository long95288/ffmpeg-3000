//
// Created by root on 2024/8/22.
// read FLTP 44100 audio file and encode to aac & mp3
// fltp_44100.pcm
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/adts_parser.h"
#include "libavutil/avutil.h"

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


static void encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt,
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
    //
    int ret;
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFrame *frame;
    AVPacket *pkt;
    AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_MONO;
    
    codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }
    c->bit_rate = 129 * 1000;
    c->sample_fmt = AV_SAMPLE_FMT_FLTP;
    c->sample_rate = 44100;
    c->profile = FF_PROFILE_AAC_LOW;
    c->ch_layout = ch_layout;

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
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
    
    
    frame->nb_samples = 1024;
    frame->format = c->sample_fmt;
    av_channel_layout_copy(&frame->ch_layout, &c->ch_layout);
    
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
    
    FILE *input_fp = fopen("./fltp_44100.pcm", "rb");
    FILE *output_fp = fopen("./fltp_44100.aac", "wb");
    int pts = 0;
    int nb = av_get_bytes_per_sample(frame->format);
    while ((ret = fread(frame->data[0], 1, nb * frame->nb_samples, input_fp)) > 0) {
        frame->nb_samples = ret / nb;
        frame->pts = (++pts);
        // send frame
        encode(c, frame, pkt, output_fp);
        ret = av_frame_make_writable(frame);
        if (ret < 0){
            fprintf(stderr, "Error av_frame_make_writable\n");
            exit(1);
        }
    }
    // handle receive last frame
    encode(c, NULL, pkt, output_fp);
    
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_close(c);
    avcodec_free_context(&c);
    
    fflush(output_fp);
    
    fclose(input_fp);
    fclose(output_fp);
    return 0;
}

int main(int argc,char *argv[]) {
    test_encode_acc();
    return 0;
}

