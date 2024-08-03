//
// Created by root on 2024/8/3.
//

#include "capture_audio.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

FILE *out_fp_left = NULL;
FILE *out_fp_right = NULL;

static void handle_capture_audio_data(uint8_t *buf, size_t size) {
    for (int i = 4; i <= size; i += 4) {
        fwrite(buf + i - 4, 2, 1, out_fp_left);
        fwrite(buf + i - 2, 2, 1, out_fp_right);
    }
    fflush(out_fp_left);
    fflush(out_fp_right);
}

int main(int argc,char *argv[]) {

    char filename[256] = {0};
    sprintf(filename, "output_audio_left.pcm");
    out_fp_left = fopen(filename, "wb");
    if (out_fp_left == NULL) {
        fprintf(stderr, "open file %s failed, %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }
    memset(filename, 0, sizeof filename);
    sprintf(filename, "output_audio_right.pcm");
    out_fp_right = fopen(filename, "wb");
    if (out_fp_right == NULL) {
        fprintf(stderr, "open file %s failed, %s\n", filename, strerror(errno));
        fclose(out_fp_left);
        exit(EXIT_FAILURE);
    }
    capture_audio_set_handle_data(handle_capture_audio_data);
    capture_audio_start();
    sleep(10);
    capture_audio_stop();

    fclose(out_fp_left);
    fclose(out_fp_right);
    return 0;
}
