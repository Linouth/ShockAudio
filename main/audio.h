#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define ENABLE_SDCARD

#define BUF_COUNT 4
#define BUF_SIZE 4  // Times DMA

#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN 1024
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

enum audio_source {
    SOURCE_SDCARD = 0,
    SOURCE_COUNT
};

struct buffer_struct {
    uint8_t *data;
    double weight;
};

struct audio_state {
    /* uint8_t *buffer[BUF_COUNT]; */
    /* double buffer_weight[BUF_COUNT]; */
    struct buffer_struct buffer[BUF_COUNT];
    int buffer_assigned[SOURCE_COUNT];
    int running;
};

size_t audio_write_ringbuf(uint8_t *buf, const uint8_t *data, size_t size);
void audio_task_start(struct audio_state *state);

#endif
