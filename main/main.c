#include "sdcard.h"
/* #include "bluetooth.h" */
/* #include "tone.h" */
#include "audio.h"
#include "buffer.h"
#include "audio_renderer.h"

#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s.h"

const char* TAG = "Main";

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

    buffer_t *buffers[SOURCE_COUNT];
    uint8_t *data;
    size_t bytes_read;

    renderer_init(&renderer_config);

#ifdef ENABLE_SDCARD
    sd_task_start();
    buffers[SOURCE_SDCARD] = sd_get_buffer();
    sd_open_file("/sdcard/test.wav");
#endif

#ifdef ENABLE_BLUETOOTH
    bt_task_start(&state);
    /* buffers[SOURCE_BLUETOOTH] = bt_get_buffer(); */
#endif

#ifdef ENABLE_TONE
    tone_task_start(&state);
    /* buffers[SOURCE_TONE] = tone_get_buffer(); */
#endif

    /**
     * Main audio loop. Retrieve data from audio source buffers,
     * reformat it and send it to the mixer
     */
    for (;;) {
        for (int i = 0; i < SOURCE_COUNT; i++) {
            ESP_LOGD(TAG, "Trying buffer %d", i);
            data = (uint8_t *)xRingbufferReceive(buffers[i]->data, &bytes_read, 0);
            
            if (bytes_read > 0 && data) {
                // TODO: recode data
                // TODO: Send data to mixer

                vRingbufferReturnItem(buffers[i]->data, data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return 0;
}
