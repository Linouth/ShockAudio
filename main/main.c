#include "sdcard.h"

#include <math.h>
#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"
#include "esp_log.h"

#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN 1024
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

static const char* TAG = "I2S";

static const int i2s_num = 0;

void i2s_init() {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 8000,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB,
        /* .dma_buf_count = 8, */
        /* .dma_buf_len = 64, */
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        /* .intr_alloc_flags = 0 */
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    ESP_LOGI(TAG, "Initializing I2S");

    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    i2s_set_pin(i2s_num, &pin_config);
}

void i2s_destroy() {
    ESP_LOGI(TAG, "Destroying I2S");

    i2s_driver_uninstall(i2s_num);
}

esp_err_t app_main(void) {
    i2s_init();
    sd_init();

    // Open file to play back (This is ugly...)
    int ret = sd_open_file("/sdcard/test.wav");
    if (ret != 0) {
        ESP_LOGE(TAG, "Could not open file");
        return 1;
    }

    // Total size of DMA buffer in bytes (Stereo)
    const int DMA_SIZE = DMA_BUF_LEN * DMA_BUF_COUNT * BITS_PER_SAMPLE * 2;

    int16_t* buf = calloc(DMA_SIZE, sizeof(char));
    size_t bytes_read, bytes_written;
    if (!buf) {
        ESP_LOGE(TAG, "Could not allocate memory..???");
        return -1;
    }

    int max = 0;
    int16_t test;
    ESP_LOGI(TAG, "Writing to I2S Buffer");
    while ((bytes_read = sd_get_data(buf, DMA_SIZE)) == DMA_SIZE) {
        for (int i = 0; i < DMA_SIZE/2; i++) {
            if (buf[i] > max)
                max = buf[i];

            /* buf[i] = buf[i] >> 4; */
            buf[i] = buf[i] / 16;

            if (i % 1024 == 0)
                printf("%d\n", buf[i]);
        }
        i2s_write(i2s_num, buf, DMA_SIZE, &bytes_written, portMAX_DELAY);
    }

    printf("Max: %d\n", max);


    ESP_LOGI(TAG, "Finished, freeing buffer");
    i2s_zero_dma_buffer(i2s_num);
    free(buf);

    return 0;
}
