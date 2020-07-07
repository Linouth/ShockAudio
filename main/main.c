#include "audio_element.h"
#include "sdcard_stream.h"

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char TAG[] = "MAIN";



esp_err_t _process(audio_element_t *self) {
    ESP_LOGI(TAG, "[%s] Processing!", self->tag);

    vTaskDelay(pdMS_TO_TICKS(1000));
    return ESP_OK;
}


esp_err_t app_main(void) {
    sdcard_stream_cfg_t cfg = DEFAULT_SDCARD_STREAM_CFG();
    audio_element_t *el = sdcard_stream_init(&cfg);

    el->open(el, "/sdcard/strobe.wav");

    vTaskDelay(pdMS_TO_TICKS(2000));
    audio_element_pause(el);

    vTaskDelay(pdMS_TO_TICKS(2000));
    audio_element_play(el);

    vTaskDelay(pdMS_TO_TICKS(2000));


    audio_element_msg_t msg = audio_element_msg_create(AEL_MSG_STOP, 0);
    audio_element_msg_send(el, &msg);
    
    return 0;
}
