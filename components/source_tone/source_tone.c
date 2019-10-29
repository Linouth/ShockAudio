#include "source_tone.h"
#include "audio_source.h"
#include "audio_buffer.h"
//TODO: Temp
#include "audio_renderer.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "math.h"

static const char* TAG = "Tone";

static xTaskHandle s_tone_task_handle = NULL;
static xQueueHandle s_tone_queue = NULL;

static source_state_t *s_state;


static void tone_task(void *arg) {
    uint8_t *data = calloc(TONE_BUF_SIZE, sizeof(char));
    size_t bytes_to_write;

    tone_t *tone = NULL;
    uint32_t val;
    size_t bytes_per_sample;
    int i;

    for (;;) {
        // Wait if not running
        // TODO: This can probaly be cone with an interrupt
        if (s_state->status != PLAYING) {
            vTaskDelay(100/portTICK_PERIOD_MS);
            continue;
        }

            
        bytes_to_write = 0;
        if ((tone && tone->duration > 0) || xQueueReceive(s_tone_queue, &tone, portMAX_DELAY)) {
            // Tone available, generate data
            bytes_per_sample = tone->bits_per_sample/8 * tone->channels;

            i = 0;
            while (tone->duration > 0 && i < TONE_BUF_SIZE/bytes_per_sample) {
                val = sin(tone->freq * 2*M_PI * i / tone->sample_rate) * (pow(2, tone->bits_per_sample-1)-1);
                tone->duration -= 1000.0/(double)tone->sample_rate;
                bytes_to_write += bytes_per_sample;

                switch (tone->bits_per_sample) {
                    case 16:
                        /*
                        data[4*i] = 0x0F & val;
                        data[4*i+1] = (0xF0 & val) >> 1;

                        data[4*i+2] = 0x0F & val;
                        data[4*i+3] = (0xF0 & val) >> 1;
                        */

                        data[4*i+1] = 0x0F & val;
                        data[4*i] = (0xF0 & val) >> 1;

                        data[4*i+3] = 0x0F & val;
                        data[4*i+2] = (0xF0 & val) >> 1;
                        break;
                    default:
                        ESP_LOGE(TAG, "This bitrate is not implemented yet");
                }

                i++;
            }
            printf("%lf\n", tone->duration);

            if (tone->duration <= 0) {
                ESP_LOGD(TAG, "Tone finished");
                free(tone);
                tone = NULL;
                s_state->status = STOPPED;
            }
        }

        if (bytes_to_write > 0) {
            xRingbufferSend(s_state->buffer.data, data, bytes_to_write, portMAX_DELAY);
            ESP_LOGD(TAG, "Bytes written to out_buffer: %u", bytes_to_write);
        } else {
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
    }

    // TODO: This will never run
    free(data);
}

// Duration in ms
void source_tone_play_tone(uint16_t freq, uint32_t samplerate, uint8_t bits_per_sample, uint8_t channels, double duration) {
    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not yet initialized");
        return;
    }

    tone_t *tone = calloc(1, sizeof(tone_t));
    tone->freq = freq;
    tone->sample_rate = samplerate;
    tone->bits_per_sample = bits_per_sample;
    tone->channels = channels;
    tone->duration = duration;

    ESP_LOGI(TAG, "Adding tone to queue");
    if (xQueueSendToBack(s_tone_queue, &tone, 10) != pdPASS) {
        ESP_LOGE(TAG, "Queue is full!");
    }
}

bool source_tone_play() {
    ESP_LOGI(TAG, "Playing");

    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not ready");
        return false;
    }

    s_state->status = PLAYING;
    return true;
}

bool source_tone_pause() {
    ESP_LOGI(TAG, "Pausing");

    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not ready");
        return false;
    }

    s_state->status = PAUSED;
    return true;
}

source_state_t *source_tone_init() {
    ESP_LOGI(TAG, "Initializing");

    s_state = create_source_state(SOURCE_TONE, TONE_BUF_SIZE);
    if (!s_state->buffer.data) {
        ESP_LOGE(TAG, "Could not allocate memory for source state");
        exit(1);
    }

    s_state->play = &source_tone_play;
    s_state->pause = &source_tone_pause;

    xTaskCreate(tone_task, "Tone", 2048, NULL, configMAX_PRIORITIES - 4, s_tone_task_handle);

    s_tone_queue = xQueueCreate(10, sizeof(tone_t *));
    if (!s_tone_queue)
        ESP_LOGE(TAG, "Could not create queue");

    s_state->status = INITIALIZED;
    ESP_LOGI(TAG, "Initialized");

    return s_state;
}
