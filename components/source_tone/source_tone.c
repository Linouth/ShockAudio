#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "math.h"
#include "esp_timer.h"

#include "source.h"
#include "pcm.h"
#include "source_tone.h"
#include "audio_buffer.h"
#include "audio_renderer.h"

const int8_t sin_lut[256] = {
       0,    3,    6,    9,   12,   15,   18,   21,   24,   27,   30,   33, 
      36,   39,   42,   45,   48,   51,   54,   57,   59,   62,   65,   67, 
      70,   73,   75,   78,   80,   82,   85,   87,   89,   91,   94,   96, 
      98,  100,  102,  103,  105,  107,  108,  110,  112,  113,  114,  116, 
     117,  118,  119,  120,  121,  122,  123,  123,  124,  125,  125,  126, 
     126,  126,  126,  126,  127,  126,  126,  126,  126,  126,  125,  125, 
     124,  123,  123,  122,  121,  120,  119,  118,  117,  116,  114,  113, 
     112,  110,  108,  107,  105,  103,  102,  100,   98,   96,   94,   91, 
      89,   87,   85,   82,   80,   78,   75,   73,   70,   67,   65,   62, 
      59,   57,   54,   51,   48,   45,   42,   39,   36,   33,   30,   27, 
      24,   21,   18,   15,   12,    9,    6,    3,    0,   -3,   -6,   -9, 
     -12,  -15,  -18,  -21,  -24,  -27,  -30,  -33,  -36,  -39,  -42,  -45, 
     -48,  -51,  -54,  -57,  -59,  -62,  -65,  -67,  -70,  -73,  -75,  -78, 
     -80,  -82,  -85,  -87,  -89,  -91,  -94,  -96,  -98, -100, -102, -103, 
    -105, -107, -108, -110, -112, -113, -114, -116, -117, -118, -119, -120, 
    -121, -122, -123, -123, -124, -125, -125, -126, -126, -126, -126, -126, 
    -127, -126, -126, -126, -126, -126, -125, -125, -124, -123, -123, -122, 
    -121, -120, -119, -118, -117, -116, -114, -113, -112, -110, -108, -107, 
    -105, -103, -102, -100,  -98,  -96,  -94,  -91,  -89,  -87,  -85,  -82, 
     -80,  -78,  -75,  -73,  -70,  -67,  -65,  -62,  -59,  -57,  -54,  -51, 
     -48,  -45,  -42,  -39,  -36,  -33,  -30,  -27,  -24,  -21,  -18,  -15, 
     -12,   -9,   -6,   -3
};
const size_t sin_lut_len = sizeof(sin_lut) / sizeof(sin_lut[0]);

static const char* TAG = "Tone";

static xTaskHandle s_tone_task_handle = NULL;
static xQueueHandle s_tone_queue = NULL;

static source_ctx_t *ctx;

size_t gen_sin_period(unsigned int Fs, unsigned int f, uint8_t *buf, size_t buf_len) {
    const unsigned int s = Fs / f;
    const float m = (float)s / sin_lut_len;

    if (s > buf_len)
        return 0;

    int i,
        ind = 0;
    float ratio;
    for (i = 0; i < s && i < buf_len; i++) {
        buf[i] = sin_lut[ind];
        ratio = (float)(i+1)/(ind+1);

        if (m <= 1) {
            // N > Fs//f
            if (ratio < m)
                ind += 1;
            else
                ind += ceil(1.0/m);
        } else {
            // N < Fs//f
            if (ratio >= m)
                ind++;
        }
    }

    return i;
}

static void tone_task(void *arg) {
    uint8_t *data = calloc(TONE_BUF_SIZE, sizeof(char));
    unsigned int data_len = 0;
    const uint8_t bits_per_sample = 8;  // TODO: Add more options ?

    uint8_t cycle[MAX_CYCLE_LEN];
    size_t cycle_len = 0;
    int cycle_index;

    tone_t *tone = NULL;
    unsigned int cycles_required = 0;
    unsigned int cycle_count;

    pcm_format_t *fmt = renderer_get_format();

    unsigned int i;
    for (;;) {
        // Wait if not running
        // TODO: This can probaly be cone with an interrupt
        if (ctx->status == PAUSED) {
            vTaskDelay(100/portTICK_PERIOD_MS);
            continue;
        }


        if (tone || xQueueReceive(s_tone_queue, &tone, portMAX_DELAY)) {
            // Tone available
            data_len = 0;

            // New tone, generate cycle data
            if (cycle_len == 0) {
                ESP_LOGI(TAG, "Starting tone with freq %d for %d miliseconds",
                        tone->freq, tone->duration);
                ctx->status = PLAYING;
                cycle_len = gen_sin_period(fmt->sample_rate, tone->freq, cycle,
                                           MAX_CYCLE_LEN);

                cycle_index = 0;
                cycle_count = 0;
                cycles_required = tone->duration/(1000.0/tone->freq);
                ESP_LOGI(TAG, "Needing %d cycles", cycles_required);
            }

            // While there is free space for a frame,
            // and while more cycles are needed
            // TODO: Non interleaved, and check if odd channels / buf sizes work. 
            while (TONE_BUF_SIZE - data_len >= fmt->channels
                    && cycle_count < cycles_required) {
                // Copy frame to buffer
                for (i = 0; i < fmt->channels; i++) {
                    data[data_len++] = cycle[cycle_index];
                }
                // Cycle finished
                if (++cycle_index >= cycle_len) {
                    cycle_index = 0;
                    cycle_count++;
                }
            }

            if (data_len > 0) {
                ctx->buffer.format.bits_per_sample = bits_per_sample;
                source_write(SOURCE_TONE, data, data_len);
                /* ESP_LOGD(TAG, "Bytes written to out_buffer: %u", data_len); */
            }

            if (cycle_count >= cycles_required) {
                ESP_LOGI(TAG, "Tone finished");
                free(tone);
                tone = NULL;
                ctx->status = STOPPED;
            }
        } else {
            // No tone playing
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
    }

    // TODO: This will never run
    free(data);
}

// Duration in ms
void source_tone_play_tone(uint16_t freq, uint16_t duration) {
    if (ctx->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not yet initialized");
        return;
    }

    tone_t *tone = calloc(1, sizeof(tone_t));
    tone->freq = freq;
    tone->duration = duration;

    ESP_LOGI(TAG, "Adding tone to queue");
    if (xQueueSendToBack(s_tone_queue, &tone, 10) != pdPASS) {
        ESP_LOGE(TAG, "Queue is full!");
    }
}

void source_tone_init() {
    ESP_LOGI(TAG, "Initializing");

    ctx = source_create_ctx("TONE", SOURCE_TONE, TONE_BUF_SIZE);

    xTaskCreate(tone_task, "Tone", 1750, NULL, configMAX_PRIORITIES - 4, s_tone_task_handle);

    s_tone_queue = xQueueCreate(10, sizeof(tone_t *));
    if (!s_tone_queue)
        ESP_LOGE(TAG, "Could not create queue");

    ctx->status = INITIALIZED;
    ESP_LOGI(TAG, "Initialized");
}
