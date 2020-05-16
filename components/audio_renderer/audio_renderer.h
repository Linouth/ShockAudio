#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include "pcm.h"

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"

#define BUF_COUNT 4
#define BUF_SIZE 2  // Times DMA

#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN 1024


typedef struct {
    pcm_format_t pcm_format;
    i2s_port_t i2s_num;
    i2s_pin_config_t pin_config;
} renderer_config_t;

void renderer_init(renderer_config_t *config);
void renderer_destroy();

int render_samples(uint8_t *buf, size_t buf_len);
void renderer_set_sample_rate(uint32_t sample_rate);
void renderer_clear_dma();

#endif
