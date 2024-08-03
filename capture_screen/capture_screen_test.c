//
// Created by root on 2024/3/18.
//
#include "capture_screen.h"
#include <unistd.h>
#include <stdio.h>


void handle_nalu_data(uint8_t *buf, int len) {
    static FILE *out_file = NULL;
    if (NULL == out_file) {
        out_file = fopen("output.h265", "wb");
        if (out_file == NULL) {
            printf("fopen failed\n");
            return;
        }
    }
    // 写入文件
    fwrite(buf, 1, len, out_file);
    fflush(out_file);
}
int main(int argc,char *argv[]) {

    screen_capture_set_handle_data(handle_nalu_data);
    start_capture();

    sleep(4);

    stop_capture();
    return 0;
}