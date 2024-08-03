//
// Created by root on 2024/8/3.
//

#include "capture_audio.h"
#include "libavdevice/avdevice.h"
#include <stdbool.h>
#include <pthread.h>

static handle_capture_audio_data_t g_handle = NULL;
static bool g_exit = true;
static AVFormatContext *fmt_ctx = NULL;
static AVPacket *packet = NULL;
static pthread_t hr;

void capture_audio_set_handle_data(handle_capture_audio_data_t handle) {
    g_handle = handle;
}

void *th_read_audio_frame(void *arg) {
    while (!g_exit && av_read_frame(fmt_ctx, packet) >= 0) {
        if(g_handle != NULL) {
            g_handle(packet->data, packet->size);
        }
        av_packet_unref(packet);
    }
    return NULL;
}

int capture_audio_start() {
    int ret = 0;

    avdevice_register_all();
    // 打开默认的mic输入设备

    const AVInputFormat *iformat = av_find_input_format("dshow");
    bool found_audio_device = false;
    char audio_device[256] = {0};

    ////////////////////
    AVDeviceInfoList *device_list = NULL;
    avdevice_list_input_sources(iformat, "dshow", NULL, &device_list);
    printf("%d\n", device_list->nb_devices);
    for (int i = 0; i < device_list->nb_devices; ++i) {
        printf("[%d] [%s] [%s] [%d] ", i, device_list->devices[i]->device_name,
               device_list->devices[i]->device_description,
               *device_list->devices[i]->media_types);
        if (AVMEDIA_TYPE_AUDIO == *device_list->devices[i]->media_types) {
            printf("AUDIO\n");
            sprintf(audio_device, "audio=%s", device_list->devices[i]->device_name);
            found_audio_device = true;
            break;
        } else if (AVMEDIA_TYPE_VIDEO == *device_list->devices[i]->media_types) {
            printf("VIDEO\n");
        }
    }
    avdevice_free_list_devices(&device_list);
    if (!found_audio_device) {
        fprintf(stderr,"not found audio device\n");
        return -1;
    }

    ///////////////////
    AVDictionary *option = NULL;
    av_dict_set(&option, "sample_size", "16", 0);
//    av_dict_set(&option, "sample_rate", "16000", 0);
    av_dict_set(&option, "channels","2", 0);

    ret = avformat_open_input(&fmt_ctx, audio_device, iformat, &option);
    if (ret != 0) {
        fprintf(stderr,"avformat_open_input failed\n");
        return ret;
    }
    printf("avformat open input %s success\n", audio_device);
    av_dict_free(&option);

    // 查看流信息,内部会读取一部分的数据获取到流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        avformat_close_input(&fmt_ctx);
        return ret;
    }

    // 打印流信息
    av_dump_format(fmt_ctx, 0, NULL, 0);
    AVCodecParameters *codecpar = fmt_ctx->streams[0]->codecpar;
    printf("code type  :%s\n", codecpar->codec_type == AVMEDIA_TYPE_AUDIO ? "audio" : "other");
    printf("codec      :%s\n", avcodec_get_name(codecpar->codec_id));
    printf("sample     :%s\n", av_get_sample_fmt_name(codecpar->format));
    printf("channel    :%d\n", codecpar->ch_layout.nb_channels);
    printf("sample_rate:%d\n", codecpar->sample_rate);

    // 获取一个帧,pcm数据
    packet = av_packet_alloc();

    g_exit = false;
    pthread_create(&hr, NULL, th_read_audio_frame, NULL);

    return 0;
}

int capture_audio_stop() {
    g_exit = true;
    pthread_join(hr, NULL);
    av_packet_free(&packet);
    avformat_close_input(&fmt_ctx);
    return 0;
}
