#ifndef SOURCE_TONE_H
#define SOURCE_TONE_H

#include "audio_source.h"
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

source_state_t *source_tone_init();
void source_tone_play_tone(uint16_t freq, uint32_t samplerate, uint8_t bits_per_sample, uint8_t channels, double duration);

#endif
