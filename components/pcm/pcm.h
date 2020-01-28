#ifndef PCM_H
#define PCM_H

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"


// TODO: Differentiate between Interleaved and Left Right layout
typedef struct {
    uint16_t sample_rate;
    i2s_bits_per_sample_t bits_per_sample;
    uint8_t channels;
} pcm_format_t;


// TODO: Add functions to resample and change bit depth
uint8_t *upsample(uint8_t *data, size_t data_len, size_t *out_len, pcm_format_t pcm, int to_rate);

#endif
