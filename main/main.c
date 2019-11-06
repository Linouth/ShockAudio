#include "audio_renderer.h"
#include "audio_source.h"
#include "source_sdcard.h"
#include "source_tone.h"
#include "pcm.h"

#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s.h"

static const char* TAG = "Main";

esp_err_t app_main(void) {

    renderer_config_t renderer_config = {
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .i2s_num = 0,
        .pin_config = {
            .bck_io_num = 26,
            .ws_io_num = 25,
            .data_out_num = 22,
            .data_in_num = I2S_PIN_NO_CHANGE
        }
    };

    source_state_t *states[SOURCE_COUNT];  // Change to linked list? 
    int states_len = 0;  // TODO: Think of something better

    uint8_t *data, *out;
    size_t bytes_read, upsampled_len;
    int highest_sample_rate, last_highest_sample_rate = 0;

    bool dma_cleared = false;
    bool running;

    renderer_init(&renderer_config);

#ifdef ENABLE_SDCARD
    states[states_len] = source_sdcard_init();
    states_len++;
    source_sdcard_play_file("/sdcard/strobe.wav");
    /* source_sdcard_play_file("/sdcard/test.wav"); */
#endif

#ifdef ENABLE_BLUETOOTH
    states[states_len] = source_bluetooth_init();
    states_len++;
#endif

#ifdef ENABLE_TONE
    states[states_len] = source_tone_init();
    states_len++;

    vTaskDelay(1000/portTICK_PERIOD_MS);
    source_tone_play_tone(3000, 44100, 16, 2, 1000);
#endif

    for (int i = 0; i < states_len; i++) {
        states[i]->play();
    }

    /**
     * Main audio loop. Retrieve data from audio source buffers,
     * reformat it and send it to the mixer
     */
    for (;;) {
        running = false;
        highest_sample_rate = 0;

        for (int i = 0; i < states_len; i++) {

            if (states[i]->status == PLAYING) {
                running = true;

                if (states[i]->buffer.format.sample_rate > highest_sample_rate) {
                    highest_sample_rate = states[i]->buffer.format.sample_rate;
                }

                data = (uint8_t *)xRingbufferReceive(states[i]->buffer.data, &bytes_read, 0);
                
                if (bytes_read > 0 && data) {
                    // Upsample everything (if SR is equal, data is being copied)
                    out = upsample(data, bytes_read, &upsampled_len, states[i]->buffer.format, highest_sample_rate);
                    // TODO: Change bit depth
                    // TODO: Send data to mixer

                    render_samples((int16_t *)out, upsampled_len);
                    vRingbufferReturnItem(states[i]->buffer.data, data);
                    free(out);
                    dma_cleared = false;
                }
            }
        }

        if (highest_sample_rate != last_highest_sample_rate && highest_sample_rate > 0) {
            renderer_set_sample_rate(highest_sample_rate);
            last_highest_sample_rate = highest_sample_rate;
        }

        if (!dma_cleared && !running) {
            renderer_clear_dma();
            dma_cleared = true;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return 0;
}
