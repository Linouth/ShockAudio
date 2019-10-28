#ifndef PCM_H
#define PCM_H

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"


// TODO: Differentiate between Interleaved and Left Right layout
typedef struct {
    uint16_t sample_rate;
    i2s_bits_per_sample_t bits_per_sample;
    uint8_t channels;
} pcm_format;


// TODO: Add functions to resample and change bit depth


#endif
