#ifndef SDCARD_H
#define SDCARD_H

#include "buffer.h"

#include <stdint.h>
#include <stddef.h>

#define SDREAD_BUF_SIZE 2048

void sd_task_start();
int sd_open_file(char* filename);
buffer_t *sd_get_buffer();

#endif
