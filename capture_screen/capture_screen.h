#ifndef SIMPLE_FFMPEG_CAPTURE_SCREEN_H
#define SIMPLE_FFMPEG_CAPTURE_SCREEN_H
#include <stddef.h>
#include <stdint.h>

typedef void (*handle_encode_data_t)(uint8_t *buf, int len);

void capture_screen_set_handle_data(handle_encode_data_t handle);

int capture_screen_start();

int capture_screen_stop();

#endif //SIMPLE_FFMPEG_CAPTURE_SCREEN_H
