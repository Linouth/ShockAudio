#include "sdcard.h"
#include "bluetooth.h"
#include "audio.h"

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
    /* state.buffer_assigned[SOURCE_SDCARD] = 0; */
    state.buffer_assigned[SOURCE_BLUETOOTH] = 0;

    /* sd_task_start(&state); */
    /* sd_open_file("/sdcard/test.wav"); */

    bt_task_start(&state);

    audio_task_start(&state);

    // TODO: Proper system to check stuff like this
    double total_weight = 0;
    for (int i = 0; i < BUF_COUNT; i++) {
        total_weight += state.buffer[i].weight;
    }
    if (total_weight > 1)
        ESP_LOGW(TAG, "WARING: Combined weight of the buffers is %f", total_weight);

    // TODO: Some main control loop
    while (state.running)
        vTaskDelay(5000/portTICK_PERIOD_MS);

    return 0;
}
