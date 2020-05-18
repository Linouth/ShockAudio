#ifndef SOURCE_TONE_H
#define SOURCE_TONE_H

#include "pcm.h"

#include <stdint.h>
#include <stddef.h>

#define TONE_BUF_SIZE 2048*2
#define MAX_CYCLE_LEN 1024

typedef struct {
    uint8_t *data;
    size_t size;
} cycle_t;

typedef struct {
    uint16_t freq;
    uint16_t duration;
} tone_t;

void source_tone_init();
void source_tone_play_tone(uint16_t freq, uint16_t duration);

#endif
