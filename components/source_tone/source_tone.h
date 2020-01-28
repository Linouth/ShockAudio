#ifndef SOURCE_TONE_H
#define SOURCE_TONE_H

#include "audio_source.h"
#include "audio_buffer.h"
#include "pcm.h"

#include <stdint.h>
#include <stddef.h>

#define TONE_BUF_SIZE 2048*8

typedef enum {
    SQUARE,
    /* TRIANGLE, */
} wave_t;

typedef struct {
    uint8_t *data;
    size_t size;
} cycle_t;

typedef struct {
    uint16_t freq;
    uint16_t duration;
    pcm_format_t pcm_format;
    wave_t type;
    cycle_t cycle;
} tone_t;

source_state_t *source_tone_init();
void source_tone_play_tone(wave_t type, uint16_t freq, uint16_t duration, pcm_format_t *pcm_format);

#endif
