//
// Created by root on 2024/8/3.
//

#ifndef FFMPEG_3000_CAPTURE_AUDIO_H
#define FFMPEG_3000_CAPTURE_AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef void (*handle_capture_audio_data_t)(uint8_t *buf, size_t len);

void capture_audio_set_handle_data(handle_capture_audio_data_t handle);

int capture_audio_start();

int capture_audio_stop();

#endif //FFMPEG_3000_CAPTURE_AUDIO_H
