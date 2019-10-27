#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"

#define BUF_COUNT 4
#define BUF_SIZE 2  // Times DMA

#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN 1024
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT


typedef struct {
    uint16_t sample_rate;
    i2s_bits_per_sample_t bits_per_sample;
    i2s_port_t i2s_num;
    i2s_pin_config_t pin_config;
} renderer_config_t;

void renderer_init(renderer_config_t *config);
void renderer_destroy();

int render_samples(int16_t *buf, size_t buf_len);
void renderer_set_sample_rate(uint32_t sample_rate);

#endif
