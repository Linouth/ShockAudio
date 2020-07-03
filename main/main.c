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

    /*
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CFG();
    cfg.process = _process;
    cfg.tag = "Test";
    */

    sdcard_stream_cfg_t cfg = DEFAULT_SDCARD_STREAM_CFG();
    audio_element_t *el = sdcard_stream_init(&cfg);



    // Wait 5 sec, and send STOP message
    vTaskDelay(pdMS_TO_TICKS(5000));

    msg_t msg = { 0 };
    msg.id = AEL_MSG_STOP;
    audio_element_msg_send(el, &msg);
    
    return 0;
}
