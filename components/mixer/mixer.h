#ifndef MIXER_H
#define MIXER_H

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include "audio_source.h"

#define MIXER_BUF_LEN 2048*8

void mixer_add_source(source_state_t *state);
RingbufHandle_t mixer_init();

#endif
