#include "audio_renderer.h"
#include "source.h"
#include "source_sdcard.h"
#include "source_tone.h"
#include "source_bluetooth.h"
#include "pcm.h"
#include "mixer.h"

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
        .pcm_format = {
            .sample_rate = 44100,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channels = 2
        },
        .i2s_num = 0,
        .pin_config = {
            .bck_io_num = 26,
            .ws_io_num = 25,
            .data_out_num = 22,
            .data_in_num = I2S_PIN_NO_CHANGE
        }
    };

    renderer_init(&renderer_config);

#ifdef ENABLE_SDCARD
    source_sdcard_init();
    source_sdcard_play_file("/sdcard/strobe.wav");
    /* source_sdcard_play_file("/sdcard/test.wav"); */
#endif

#ifdef ENABLE_BLUETOOTH
    source_bluetooth_init();
#endif

#ifdef ENABLE_TONE
    source_tone_init();

    vTaskDelay(15000/portTICK_PERIOD_MS);
    source_tone_play_tone(440, 10000);
#endif

    // Mixer takes data directly from the source ctxs list, and sends it to the
    // renderer.
    // TODO: Split this, so that the final data is available to other components.
    mixer_init();

    /*
    for (int i = 0; i < states_len; i++) {
        states[i]->play();
    }
    */

    /**
     * Is going to be used for things like a webserver / syncing other nodes
     * Maybe.
     */
    for (;;) {
            vTaskDelay(pdMS_TO_TICKS(10000));
    }

    return 0;
}
