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

static int16_t *output_buffer = NULL;

// TODO: Add function to combine buffers if needed

void i2s_init() {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 44100,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        /* .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB, */
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
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
    struct audio_state *state = (struct audio_state *) arg;

    int16_t* data;
    size_t bytes_received;
    size_t bytes_received_max = 0;
    size_t bytes_written;
    int i, j;

    for (;;) {
        // Clear output buffer
        // TODO: Dit kan netter en sneller...
        for (i = 0; i < bytes_received_max/2; i++) {
            output_buffer[i] = 0;
        }
        bytes_received_max = 0;
        // Mix buffers
        for (i = 0; i < BUF_COUNT; i++) {
            if (state->buffer[i].weight != 0) {  // Skip unused buffers
                ESP_LOGD(TAG, "Trying buffer %d", i);
                data = (int16_t *) xRingbufferReceive(state->buffer[i].data, &bytes_received, 500/portTICK_PERIOD_MS); 

                if (bytes_received != 0 && data != NULL) {
                    ESP_LOGD(TAG, "Received %u bytes from buffer %d", bytes_received, i);
                    printf("data: %p\noutputbuffer: %p\n", data, output_buffer);
                    for (j = 0; j < bytes_received/2; j++) {
                        output_buffer[j] += data[j] * state->buffer[i].weight;
                    }
                    

                    if (bytes_received > bytes_received_max)
                        bytes_received_max = bytes_received;
                    vRingbufferReturnItem(state->buffer[i].data, data);
                }
            }
        }

        if (bytes_received_max != 0) {
            ESP_LOGD(TAG, "Writing %u bytes to DMA buffer", bytes_received_max);
            // TODO: Proper volume control
            for (i = 0; i < bytes_received_max/2; i++) { // Hakcy way to reduce volume
                output_buffer[i] >>= 4;
            }
            i2s_write(i2s_num, output_buffer, bytes_received_max, &bytes_written, portMAX_DELAY);
        } else {
            ESP_LOGD(TAG, "Buffer empty, clearing DMA");
            i2s_zero_dma_buffer(i2s_num);
            vTaskDelay(1000/portTICK_PERIOD_MS);
        }
    }

    // TODO: This will never run
    free(output_buffer);
}

size_t audio_write_ringbuf(uint8_t *buf, const uint8_t *data, size_t size) {
    BaseType_t done = xRingbufferSend(buf, data, size, portMAX_DELAY);
    if (done)
        return size;
    else
        return 0;
} 

void audio_task_start(struct audio_state *state) {
    // Total size of DMA buffer in bytes (Stereo)
    const int DMA_SIZE = DMA_BUF_LEN * DMA_BUF_COUNT * (BITS_PER_SAMPLE/8) * 2;

    // Initialize buffers
    for (int i = 0; i < BUF_COUNT; i++) {
        state->buffer[i].data = xRingbufferCreate(BUF_SIZE*DMA_SIZE, RINGBUF_TYPE_BYTEBUF);
        if (!state->buffer[i].data) {
            ESP_LOGE(TAG, "Could not create Ringbuffer %d", i);
            return;
        }
    }
    output_buffer = calloc(DMA_SIZE * BUF_SIZE, sizeof(char));
    if (!output_buffer) {
        ESP_LOGE(TAG, "Could not allocate memory for audio output_buffer");
        abort();
    }

    i2s_init();

    xTaskCreate(audio_task, "Audio", DMA_SIZE/4, state, configMAX_PRIORITIES-3, &s_audio_handle);
}
