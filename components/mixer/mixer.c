#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"

#include "mixer.h"
#include "audio_source.h"
#include "audio_buffer.h"
#include "audio_renderer.h"


static const char* TAG = "Mixer";

static xTaskHandle s_mixer_task_handle = NULL;

static source_state_t *s_states[SOURCE_COUNT] = {NULL};
static unsigned int s_states_len = 0;

static void mixer_task(void *params) {
    source_state_t *state;
    uint8_t *out_buf = calloc(MIXER_BUF_LEN, sizeof(char));
    int8_t *data;

    int32_t tmp;
    int32_t *samples = calloc(MIXER_BUF_LEN, sizeof(int32_t));  // Mono samples (so L, R, L, R with stereo)
    size_t sample_index;
    size_t sample_count;

    size_t bytes_read;
    size_t max_bytes_read;
    size_t bytes_per_sample;
    size_t max_bytes_per_sample;

    source_state_t *states_playing[SOURCE_COUNT];
    size_t states_count;

    bool dma_cleared = false;
    
    int i,j,k;
    for (;;) {
        max_bytes_read = 0;
        max_bytes_per_sample = 0;
        states_count = 0;
        sample_count = 0;

        // Check which states are playing
        for (i = 0; i < s_states_len; i++) {
            if (s_states[i]->status == PLAYING) {
                states_playing[states_count++] = s_states[i];

                // TODO: Keep track of highest sample rate and bits per sample
            }
        }

        for (i = 0; i < states_count; i++) {
            state = states_playing[i];
            sample_index = 0;

            // Expected format:  data[..] = { left[7:0], left[15:8], right[7:0], right[15:8],left[7:0], ... } 
            data = (int8_t *) xRingbufferReceive(state->buffer.data, &bytes_read, portMAX_DELAY);
            max_bytes_read = (bytes_read > max_bytes_read) ?
                bytes_read : max_bytes_read;
            bytes_per_sample = (state->buffer.format.bits_per_sample > 16) ?
                4 : (state->buffer.format.bits_per_sample/8);
            max_bytes_per_sample = (bytes_per_sample > max_bytes_per_sample) ?
                bytes_per_sample : max_bytes_per_sample;

            if (bytes_read > 0 && data) {

                // TODO: Move this into a separate helper function to pack and unpack the data into signed numbers
                for (j = 0; j < bytes_read; j+=bytes_per_sample) {

                    // Convert bytes to single signed integer
                    tmp = 0;
                    for (k = 0; k < bytes_per_sample; k++) {
                        tmp |= (data[j+k] << (8*k)) & (0xff << (8*k));
                    }
                    tmp |= data[j+bytes_per_sample-1] << (8*(bytes_per_sample-1));

                    // Process data
                    tmp >>= 2;

                    samples[sample_index++] += tmp;
                }
                sample_count = (sample_index > sample_count) ? sample_index : sample_count;

                vRingbufferReturnItem(state->buffer.data, data);
            }
        }
        
        if (max_bytes_read > 0) {
            // TODO: Move this into a separate helper function to pack and unpack the data into signed numbers
            for (i = 0; i < sample_count; i++) {
                for (j = 0; j < max_bytes_per_sample; j++) {
                    out_buf[i*max_bytes_per_sample + j] = (samples[i]>>(8*j) & 0xff);
                }

                samples[i] = 0;
            }
            render_samples((int16_t *)out_buf, max_bytes_read);
            dma_cleared = false;
        } else {
            // No data written
            if (!dma_cleared) {
                renderer_clear_dma();
                dma_cleared = true;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    free(out_buf);
    free(samples);
}


void mixer_add_source(source_state_t *state) {
    ESP_LOGI(TAG, "Adding source: %d", state->source);
    s_states[s_states_len] = state;
    s_states_len++;
}


void mixer_init() {
    ESP_LOGI(TAG, "Initializing");

    xTaskCreate(mixer_task, "Mixer", 2048, NULL, configMAX_PRIORITIES-1, s_mixer_task_handle);
}
