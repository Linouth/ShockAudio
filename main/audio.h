#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define ENABLE_SDCARD

#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN 1024
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

size_t audio_write_ringbuf(const uint8_t *data, size_t size);
void audio_task_start();

#endif
