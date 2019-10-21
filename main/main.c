#include "sdcard.h"
#include "bluetooth.h"
#include "tone.h"
#include "audio.h"

#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

const char* TAG = "Main";

esp_err_t app_main(void) {

    struct audio_state state = {
        .buffer = {{0}},
        .buffer_assigned = {-1},
        .running = 1
    };

    state.buffer[0].weight = 1.0;
    state.buffer[1].weight = 1.0;
    state.buffer[2].weight = 1.0;
    state.buffer_assigned[SOURCE_SDCARD] = 0;
    state.buffer_assigned[SOURCE_BLUETOOTH] = 1;
    state.buffer_assigned[SOURCE_TONE] = 2;

    audio_task_start(&state);

#ifdef ENABLE_SDCARD
    sd_task_start(&state);
    sd_open_file("/sdcard/test.wav");
#endif

#ifdef ENABLE_BLUETOOTH
    bt_task_start(&state);
#endif

#ifdef ENABLE_TONE
    tone_task_start(&state);
#endif

    // TODO: Proper system to check stuff like this
    /*
    double total_weight = 0;
    for (int i = 0; i < BUF_COUNT; i++) {
        total_weight += state.buffer[i].weight;
    }
    if (total_weight > 1)
        ESP_LOGW(TAG, "WARING: Combined weight of the buffers is %f", total_weight);
    */

    // TODO: Some main control loop
    while (state.running) {
        tone_gen();
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }

    return 0;
}
