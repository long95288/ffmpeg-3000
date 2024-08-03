#ifndef SIMPLE_FFMPEG_CAPTURE_SCREEN_H
#define SIMPLE_FFMPEG_CAPTURE_SCREEN_H
#include <stddef.h>
#include <stdint.h>

typedef void (*handle_encode_data_t)(uint8_t *buf, int len);

void screen_capture_set_handle_data(handle_encode_data_t handle);

int start_capture();

int stop_capture();

#endif //SIMPLE_FFMPEG_CAPTURE_SCREEN_H
