#ifndef SOURCE_SDCARD_H
#define SOURCE_SDCARD_H

#include "audio_buffer.h"

#include <stdint.h>
#include <stddef.h>

#define SDREAD_BUF_SIZE 2048*4

int source_sdcard_play_file(char* filename);
void source_sdcard_init();
int source_sdcard_start();
buffer_t *source_sdcard_get_buffer();

#endif
