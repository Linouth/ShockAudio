#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"

#include "mixer.h"
#include "audio_source.h"
#include "audio_buffer.h"


static const char* TAG = "Mixer";

static xTaskHandle s_mixer_task_handle = NULL;
static RingbufHandle_t s_outbuffer = NULL;

static source_state_t *s_states[SOURCE_COUNT] = {NULL};
static unsigned int s_states_len = 0;

static void mixer_task(void *params) {
    source_state_t *state;
    uint8_t *out_buf = calloc(MIXER_BUF_LEN, sizeof(char));
    uint8_t *data;
    int16_t tmp1, tmp2;
    size_t bytes_read;
    size_t max_bytes_read;
    
    int i,j;
    for (;;) {
        max_bytes_read = 0;
        for (i = 0; i < s_states_len; i++) {
            state = s_states[i];

            if (state->status == PLAYING) {
                data = (uint8_t *)xRingbufferReceive(state->buffer.data, &bytes_read, portMAX_DELAY);

                max_bytes_read = (bytes_read > max_bytes_read) ? bytes_read : max_bytes_read;
                if (bytes_read > 0 && data) {
                    for (j = 0; j < bytes_read; j+=2) {
                        tmp1 = data[j]<<8 & data[j+1];
                        /* tmp2 = out_buf[j+1]<<8 & out_buf[j]; */
                        /* out_buf[j] = (out_buf[j] >> 1) + (data[j] >> 1); */
                        /* out_buf[j] = data[j]; */
                        /* out_buf[j] = tmp1 >> 8; */
                        /* out_buf[j+1] = tmp1 & 0x00ff; */
                        out_buf[j] = data[j];
                        out_buf[j+1] = data[j+1];
                    }

                    vRingbufferReturnItem(state->buffer.data, data);
                }
            }
        }

        if (max_bytes_read > 0) {
            xRingbufferSend(s_outbuffer, out_buf, max_bytes_read, portMAX_DELAY);
            /* ESP_LOGI(TAG, "Bytes written to out_buffer: %u", max_bytes_read); */
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    free(data);
}


void mixer_add_source(source_state_t *state) {
    ESP_LOGI(TAG, "Adding source: %d", state->source);
    s_states[s_states_len] = state;
    s_states_len++;
}


RingbufHandle_t mixer_init() {
    ESP_LOGI(TAG, "Initializing");

    s_outbuffer = xRingbufferCreate(MIXER_BUF_LEN, RINGBUF_TYPE_BYTEBUF);
    if (!s_outbuffer) {
        ESP_LOGE(TAG, "Could not create output buffer");
        return NULL;
    }

    xTaskCreate(mixer_task, "Mixer", 2048, NULL, configMAX_PRIORITIES-1, s_mixer_task_handle);

    return s_outbuffer;
}
