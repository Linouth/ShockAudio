#include "sdcard.h"
#include "audio.h"

#include "esp_err.h"
#include "esp_log.h"

esp_err_t app_main(void) {

    sd_task_start();
    sd_open_file("/sdcard/test.wav");
    audio_task_start();

    return 0;
}
