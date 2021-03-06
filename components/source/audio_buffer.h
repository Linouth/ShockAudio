#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include "pcm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#define BUFFER_SIZE 2048*2

typedef struct {
    RingbufHandle_t data;
    pcm_format_t format;
    uint8_t channel;
} buffer_t;


buffer_t *create_buffer(size_t len);
void clear_buffer(buffer_t *buf);

#endif
