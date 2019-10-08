#include "sdcard.h"
#include "audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

const char* TAG = "Main";

esp_err_t app_main(void) {

    struct audio_state state = {
        .buffer_weight = {0},
        .buffer_assigned = {-1},
        .running = 1
    };

    state.buffer_weight[0] = 1;
    state.buffer_assigned[SOURCE_SDCARD] = 0;

    audio_task_start(&state);
    sd_task_start(&state);
    sd_open_file("/sdcard/test.wav");

    // TODO: Proper system to check stuff like this
    double total_weight = 0;
    for (int i = 0; i < BUF_COUNT; i++) {
        total_weight += state.buffer_weight[i];
    }
    if (total_weight > 1)
        ESP_LOGW(TAG, "WARING: Combined weight of the buffers is %f", total_weight);

    // TODO: Some main control loop
    while (state.running)
        vTaskDelay(5000/portTICK_PERIOD_MS);

    return 0;
}
