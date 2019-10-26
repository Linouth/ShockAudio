#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "driver/i2s.h"

enum audio_source {
#ifdef ENABLE_SDCARD
    SOURCE_SDCARD = 0,
#endif
#ifdef ENABLE_BLUETOOTH
    SOURCE_BLUETOOTH,
#endif
#ifdef ENABLE_TONE
    SOURCE_TONE,
#endif
    SOURCE_COUNT
};

// TODO: Differentiate between Interleaved and Left Right layout
typedef struct {
    uint16_t sample_rate;
    i2s_bits_per_sample_t bits_per_sample;
    uint8_t channels;
} pcm_format;

#endif
