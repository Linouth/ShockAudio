#include "sdcard.h"
#include "audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/i2s.h"
#include "esp_log.h"


static const char* TAG = "Audio";
static const int i2s_num = 0;

static xTaskHandle s_audio_handle = NULL;
static RingbufHandle_t s_ringbuf = NULL;

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

    ESP_LOGI(TAG, "Initialization done");
}

void i2s_destroy() {
    ESP_LOGI(TAG, "Destroying I2S");

    i2s_driver_uninstall(i2s_num);
}

void audio_task(void *arg) {
    uint8_t* data;
    size_t bytes_received;
    size_t bytes_written;
    for (;;) {
        data = (uint8_t *) xRingbufferReceive(s_ringbuf, &bytes_received, portMAX_DELAY);
        if (bytes_received != 0) {
            ESP_LOGD(TAG, "Received %u bytes for DMA buffer", bytes_received);
            i2s_write(i2s_num, data, bytes_received, &bytes_written, portMAX_DELAY);
            vRingbufferReturnItem(s_ringbuf, data);
        }
    }
}

size_t audio_write_ringbuf(const uint8_t *data, size_t size) {
    BaseType_t done = xRingbufferSend(s_ringbuf, data, size, portMAX_DELAY);
    if (done)
        return size;
    else
        return 0;
} 

void audio_task_start() {
    // Total size of DMA buffer in bytes (Stereo)
    const int DMA_SIZE = DMA_BUF_LEN * DMA_BUF_COUNT * (BITS_PER_SAMPLE/8) * 2;

    s_ringbuf = xRingbufferCreate(4*DMA_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!s_ringbuf) {
        ESP_LOGE(TAG, "Could not create Ringbuffer");
        return;
    }

    i2s_init();

    // TODO: Also fix stack size
    xTaskCreate(audio_task, "Audio", DMA_SIZE, NULL, configMAX_PRIORITIES-3, &s_audio_handle);
}
