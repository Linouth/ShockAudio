#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "audio_element.h"
#include "sdcard_stream.h"
#include "sdcard.h"

#include "esp_err.h"
#include "esp_log.h"

static const char TAG[] = "SDCARD_STREAM";


typedef struct sdcard_stream {
    FILE *file;
} sdcard_stream_t;


static esp_err_t _sdcard_open(audio_element_t *el) {
    return ESP_OK;
}


static esp_err_t _sdcard_close(audio_element_t *el) {
    return ESP_OK;
}


static esp_err_t _sdcard_destroy(audio_element_t *el) {
    sdcard_destroy();

    if (el->data)
        free(el->data);

    return ESP_OK;
}


static esp_err_t _sdcard_process(audio_element_t *el) {
    ESP_LOGI(TAG, "[%s] Processing!", el->tag);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    return ESP_OK;
}

static size_t _sdcard_read(audio_element_t *el, char *buf, size_t len) {
    return 0;
}

esp_err_t audio_element_msg_handler(audio_element_t *el, msg_t *msg);
static esp_err_t _sdcard_msg_handler(audio_element_t *el, msg_t *msg) {
    switch (msg->id) {
        default:
            ESP_LOGD(TAG, "[%s] Unhandled message, passing to general handler",
                    el->tag);
            return audio_element_msg_handler(el, msg);
    }

    return ESP_OK;
}

audio_element_t *sdcard_stream_init(sdcard_stream_cfg_t *config) {
    audio_element_t *el;
    sdcard_stream_t *sdcard = calloc(1, sizeof(sdcard_stream_t));
    if (!sdcard) {
        ESP_LOGE(TAG, "Could not allocate memory.");
        return NULL;
    }

    sdcard_init("/sdcard", 5);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CFG();
    cfg.open = _sdcard_open;
    cfg.close = _sdcard_close;
    cfg.destroy = _sdcard_destroy;
    cfg.process = _sdcard_process;
    cfg.msg_handler = _sdcard_msg_handler;

    cfg.buf_size = config->buf_size;
    cfg.out_rb_size = config->out_rb_size;
    cfg.task_stack = config->task_stack;

    cfg.tag = "sdcard";

    if (config->type == AEL_STREAM_READER) {
        cfg.read = _sdcard_read;
    } else {
        // TODO: Add support for file writing
        // AEL_STREAM_WRITER;
        ESP_LOGE(TAG, "AEL_STREAM_WRITER Not implemented yet.");
        return NULL;
    }

    el = audio_element_init(&cfg);
    if (!el) {
        ESP_LOGE(TAG, "[%s] Could not allocate memory for audio element.",
                cfg.tag);
    }
    el->data = sdcard;

    return el;
}
