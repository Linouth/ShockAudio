#ifndef BUFFER_H
#define BUFFER_H

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include "audio.h"

#define BUFFER_SIZE 2048

typedef struct {
    RingbufHandle_t data;
    pcm_format format;
} buffer_t;


buffer_t *create_buffer(size_t len);
void clear_buffer(buffer_t *buf);

#endif
