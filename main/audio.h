#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#define BUF_COUNT 4
#define BUF_SIZE 2  // Times DMA

#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN 1024
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

enum audio_source {
    SOURCE_SDCARD = 0,
    SOURCE_BLUETOOTH,
    SOURCE_TONE,
    SOURCE_COUNT
};

struct buffer_struct {
    RingbufHandle_t *data;
    double weight;
    int sample_rate;
};

struct audio_state {
    struct buffer_struct buffer[BUF_COUNT];
    int buffer_assigned[SOURCE_COUNT];

    int sample_rate;
    int bits_per_sample;

    int running;
};

size_t audio_write_ringbuf(const uint8_t *data, size_t size, const enum audio_source source);
void audio_task_start(struct audio_state *state);

#endif
