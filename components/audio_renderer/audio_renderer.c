#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"
#include "esp_log.h"

#include "audio_renderer.h"

static const char* TAG = "Renderer";
static int i2s_num = 0;


static void i2s_init(renderer_config_t *config) {
    i2s_num = config->i2s_num;

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = config->sample_rate,
        .bits_per_sample = config->bits_per_sample,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,  // Maybe
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };

    ESP_LOGI(TAG, "Initializing I2S");

    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    i2s_set_pin(i2s_num, &config->pin_config);

    ESP_LOGI(TAG, "Initialization done");
}

static void i2s_destroy() {
    ESP_LOGI(TAG, "Destroying I2S");

    i2s_driver_uninstall(i2s_num);
}

void renderer_init(renderer_config_t *config) {
    i2s_init(config);
}

void renderer_destroy() {
    i2s_destroy();
}

int render_samples(int16_t *buf, size_t buf_len) {
    if (!buf) {
        ESP_LOGE(TAG, "Received a invalid buffer");
        return -1;
    }

    size_t bytes_written;

    ESP_LOGD(TAG, "Writing %u bytes to DMA buffer", buf_len);
    // TODO: Remove this
    for (int i = 0; i < buf_len/2; i++) { // Hakcy way to reduce volume
        buf[i] >>= 4;
    }
    i2s_write(i2s_num, buf, buf_len, &bytes_written, portMAX_DELAY);

    return bytes_written;
}

void renderer_set_sample_rate(uint32_t sample_rate) {
    ESP_LOGI(TAG, "Setting sample rate to %d", sample_rate);
    i2s_set_sample_rates(i2s_num, sample_rate);
}

void renderer_clear_dma() {
    ESP_LOGI(TAG, "Clearing DMA buffer");
    i2s_zero_dma_buffer(i2s_num);
}
