#include "sdcard.h"
#include "audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

esp_err_t app_main(void) {

    struct audio_state state = {
        .bufferWeight = {0},
        .bufferAssigned = {-1},
        .running = 1
    };

    state.bufferWeight[0] = 1;
    state.bufferAssigned[SOURCE_SDCARD] = 0;

    audio_task_start(&state);
    sd_task_start(&state);
    sd_open_file("/sdcard/test.wav");

    // Some main control loop
    while (state.running)
        vTaskDelay(5000/portTICK_PERIOD_MS);

    return 0;
}
