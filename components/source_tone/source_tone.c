#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "math.h"
#include "esp_timer.h"

#include "source_tone.h"
/* #include "audio_source.h" */
#include "source.h"
#include "audio_buffer.h"
#include "pcm.h"


static const char* TAG = "Tone";

static xTaskHandle s_tone_task_handle = NULL;
static xQueueHandle s_tone_queue = NULL;

static source_state_t *s_state;

static cycle_t gen_cycle(wave_t wave_type, int freq, pcm_format_t *pcm_format) {
    cycle_t cycle;
    int samples_per_cycle = pcm_format->sample_rate/freq;
    int mono_sample_size = pcm_format->bits_per_sample/8;
    int sample_size = mono_sample_size * pcm_format->channels;
    cycle.size = mono_sample_size * samples_per_cycle * pcm_format->channels;
    cycle.data = malloc(cycle.size);

    uint8_t mono_cycle[mono_sample_size * samples_per_cycle];
    
    // TODO: Proper signed support (-2**16 til 2**16)
    switch(wave_type) {
    case SQUARE:
    {
        memset(mono_cycle, 0, mono_sample_size*samples_per_cycle/2);
        memset(mono_cycle+mono_sample_size*(samples_per_cycle - samples_per_cycle/2), 0x07, mono_sample_size*(samples_per_cycle - samples_per_cycle/2));
        break;
    }
    default:
        ESP_LOGW(TAG, "Wavetype %d is not implemented yet", wave_type);
        break;
    }

    // TODO: Add support for non stereo tones (not sure when this is ever needed tho)
    // Interleave left and right channel data 
    for (int i = 0; i < samples_per_cycle; i++) {
        // For every sample in cycle
        memcpy(cycle.data+i*sample_size, mono_cycle+i*mono_sample_size, mono_sample_size);
        memcpy(cycle.data+mono_sample_size+i*sample_size, mono_cycle+i*mono_sample_size, mono_sample_size);
    }

    return cycle;
}

static void tone_task(void *arg) {
    uint8_t *data = calloc(TONE_BUF_SIZE, sizeof(char));
    unsigned int bytes_to_write = 0;
    uint8_t bits_per_sample = 0;

    tone_t *tone = NULL;
    unsigned int cycles_required = 0;
    unsigned int cycle_count = 0;

    unsigned int i;
    for (;;) {
        // Wait if not running
        // TODO: This can probaly be cone with an interrupt
        if (s_state->status == PAUSED) {
            vTaskDelay(100/portTICK_PERIOD_MS);
            continue;
        }


        if (tone || xQueueReceive(s_tone_queue, &tone, portMAX_DELAY)) {
            // Tone available
            bytes_to_write = 0;
            bits_per_sample = tone->pcm_format.bits_per_sample;

            // New tone, Generate cycle data
            if (tone->cycle.size == 0) {
                ESP_LOGI(TAG, "Starting tone with freq %d for %d miliseconds", tone->freq, tone->duration);
                s_state->status = PLAYING;
                tone->cycle = gen_cycle(SQUARE, tone->freq, &tone->pcm_format);

                cycle_count = 0;
                cycles_required = tone->duration/(1000.0/tone->freq);
                ESP_LOGI(TAG, "Needing %d cycles", cycles_required);
            }

            for (i = 0; i < TONE_BUF_SIZE/tone->cycle.size && cycle_count < cycles_required; i++, cycle_count++) {
                memcpy(data+i*tone->cycle.size, tone->cycle.data, tone->cycle.size);
                bytes_to_write += tone->cycle.size;
            }

            if (cycle_count >= cycles_required) {
                ESP_LOGI(TAG, "Tone finished");
                free(tone->cycle.data);
                free(tone);
                tone = NULL;
                s_state->status = STOPPED;
            }
        }

        if (bytes_to_write > 0) {
            s_state->buffer.format.bits_per_sample = bits_per_sample;
            xRingbufferSend(s_state->buffer.data, data, bytes_to_write, portMAX_DELAY);
            /* ESP_LOGD(TAG, "Bytes written to out_buffer: %u", bytes_to_write); */
        } else {
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
    }

    // TODO: This will never run
    free(data);
}

// Duration in ms
void source_tone_play_tone(wave_t type, uint16_t freq, uint16_t duration, pcm_format_t *pcm_format) {
    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not yet initialized");
        return;
    }

    tone_t *tone = calloc(1, sizeof(tone_t));
    tone->freq = freq;
    tone->pcm_format = *pcm_format;
    tone->duration = duration;
    tone->type = type;

    ESP_LOGI(TAG, "Adding tone to queue");
    if (xQueueSendToBack(s_tone_queue, &tone, 10) != pdPASS) {
        ESP_LOGE(TAG, "Queue is full!");
    }
}

bool source_tone_play(void *param) {
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

    xTaskCreate(tone_task, "Tone", 1750, NULL, configMAX_PRIORITIES - 4, s_tone_task_handle);

    s_tone_queue = xQueueCreate(10, sizeof(tone_t *));
    if (!s_tone_queue)
        ESP_LOGE(TAG, "Could not create queue");

    s_state->status = INITIALIZED;
    ESP_LOGI(TAG, "Initialized");

    return s_state;
}
