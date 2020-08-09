#include "audio_element.h"
#include "sdcard_stream.h"
#include "i2s_stream.h"
#include "a2dp_stream.h"

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "io.h"

static const char TAG[] = "MAIN";

esp_err_t app_main(void) {
    /*
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CFG();
    cfg.buf_len = 8192*2;
    cfg.out_rb_size = 8192*2;
    cfg.task_stack = 2048;
    audio_element_t *sdcard = sdcard_stream_init(cfg, AEL_STREAM_READER);
    */

    /* sdcard->open(sdcard, "/sdcard/strobe.wav"); */

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CFG();
    cfg.buf_len = 8192;
    cfg.out_rb_size = 8192;
    cfg.task_stack = 2048;
    audio_element_t *a2dp = a2dp_stream_init(cfg, AEL_STREAM_READER);


    cfg.buf_len = 2048;
    cfg.task_stack = 2048;
    cfg.out_rb_size = 0;
    cfg.input = a2dp->output;
    audio_element_t *i2s = i2s_stream_init(cfg, AEL_STREAM_WRITER);
    i2s->open(i2s, NULL);

    a2dp->open(a2dp, "ShockSpeaker");
    ESP_LOGI(TAG, "Everything started");
    
    return 0;
}
