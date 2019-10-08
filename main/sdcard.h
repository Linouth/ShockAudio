#ifndef SDCARD_H
#define SDCARD_H

#include "audio.h"

#include <stdint.h>
#include <stddef.h>

#define SDREAD_BUF_SIZE 2048

void sd_task_start(struct audio_state *state);
int sd_open_file(char* filename);

#endif
