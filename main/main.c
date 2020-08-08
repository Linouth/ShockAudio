/* #include "audio_element.h" */
/* #include "sdcard_stream.h" */
/* #include "i2s_stream.h" */

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "io.h"

static const char TAG[] = "MAIN";

esp_err_t app_main(void) {
    /*
    sdcard_stream_cfg_t sd_cfg = DEFAULT_SDCARD_STREAM_CFG();
    audio_element_t *sdcard = sdcard_stream_init(&sd_cfg);

    ESP_LOGE(TAG, "TEST");
    i2s_stream_cfg_t i2s_cfg = DEFAULT_I2S_STREAM_CFG();
    audio_element_t *i2s = i2s_stream_init(&i2s_cfg);
    i2s->input.type = IO_TYPE_RB;
    i2s->input.data = sdcard->output.data;
    i2s->input.ready = true;
    i2s->open(i2s, NULL);

    sdcard->open(sdcard, "/sdcard/strobe.wav");
    */


    /* vTaskDelay(pdMS_TO_TICKS(2000)); */
    /* audio_element_pause(el); */

    /* vTaskDelay(pdMS_TO_TICKS(2000)); */
    /* audio_element_play(el); */

    /* vTaskDelay(pdMS_TO_TICKS(2000)); */


    /* audio_element_msg_t msg = audio_element_msg_create(AEL_MSG_STOP, 0); */
    /* audio_element_msg_send(el, &msg); */
    
    return 0;
}
