#ifndef BUFFER_H
#define BUFFER_H

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#define BUFFER_SIZE 2048

typedef struct {
    RingbufHandle_t data;
    
    uint16_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
} buffer_t;

#endif
