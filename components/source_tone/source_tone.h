#ifndef SOURCE_TONE_H
#define SOURCE_TONE_H

#include "audio_buffer.h"

#include <stdint.h>
#include <stddef.h>

#define TONE_BUF_SIZE 2048*8

typedef struct {
    uint16_t freq;
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
    double duration;
} tone_t;

void source_tone_init();
int source_tone_start();
buffer_t *source_tone_get_buffer();

void source_tone_play(uint16_t freq, uint32_t samplerate, uint8_t bits_per_sample, uint8_t channels, double duration);

#endif
