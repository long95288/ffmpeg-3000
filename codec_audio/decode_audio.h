//
// Created by root on 2024/8/4.
//

#ifndef FFMPEG_3000_DECODE_AUDIO_H
#define FFMPEG_3000_DECODE_AUDIO_H
#include <stddef.h>

int audio_decode_init();

int audio_decode_process(unsigned char *buf, size_t size);

int audio_decode_deinit();

#endif //FFMPEG_3000_DECODE_AUDIO_H
