#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "audio_element.h"
#include "sdcard_stream.h"
#include "sdcard.h"

#include "esp_err.h"
#include "esp_log.h"

static const char TAG[] = "SDCARD_STREAM";


typedef struct sdcard_stream {
    audio_stream_type_t type;
    FILE *file;
} sdcard_stream_t;



static esp_err_t _sdcard_open(audio_element_t *el, void* pv) {
    audio_element_info_t *info =  &el->info;
    sdcard_stream_t *stream = el->data;

    // Check for file to open
    char *uri = (char*)pv;
    if (!uri) {
        ESP_LOGE(TAG, "[%s] Uri not set!", el->tag);
        return ESP_FAIL;
    }

    // Check if stream already opened
    if (el->is_open) {
        ESP_LOGE(TAG, "[%s] Already open!", el->tag);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[%s] Opening %s", el->tag, uri);
    if (stream->type == AEL_STREAM_READER) {

        // Open file
        stream->file = fopen(uri, "r");
        if (!stream->file) {
            ESP_LOGE(TAG, "[%s] Could not open file", el->tag);
            return ESP_FAIL;
        }

        // Find filesize
        fseek(stream->file, 0, SEEK_END);
        info->bytes = ftell(stream->file);
        fseek(stream->file, 0, SEEK_SET);
        ESP_LOGI(TAG, "[%s] File is %d bytes", el->tag, info->bytes);
    } else {
        ESP_LOGE(TAG, "[%s] sdcard_stream only support AEL_STREAM_READER for now.",
                el->tag);
        return ESP_FAIL;
    }

    el->is_open = true;

    return ESP_OK;
}


static esp_err_t _sdcard_close(audio_element_t *el) {
    sdcard_stream_t *stream = el->data;

    if (stream->file) {
        ESP_LOGI(TAG, "[%s] Closing", el->tag);
        fclose(stream->file);
        el->is_open = false;
    }

    return ESP_OK;
}


static esp_err_t _sdcard_destroy(audio_element_t *el) {
    // TODO: Find out why this shit crashes on unmounting the fatfs...
    /* sdcard_destroy(); */

    if (el->data)
        free(el->data);

    return ESP_OK;
}


static size_t _sdcard_process(audio_element_t *el) {
    size_t bytes_read = audio_element_input(el, el->buf, el->buf_size);
    if (bytes_read > 0) {
        return audio_element_output(el, el->buf, el->buf_size);
    }
    return 0;
}


static size_t _sdcard_read(audio_element_t *el, char *buf, size_t len) {
    sdcard_stream_t *stream = el->data;
    audio_element_info_t *info = &el->info;
    int bytes_read = fread(buf, 1, len, stream->file);
    if (bytes_read > 0) {
        info->byte_pos += len;
    } else {
        ESP_LOGW(TAG, "[%s] No data left", el->tag);
    }

    return bytes_read;
}


esp_err_t audio_element_msg_handler(audio_element_t *el, audio_element_msg_t *msg);
static esp_err_t _sdcard_msg_handler(audio_element_t *el, audio_element_msg_t *msg) {
    switch (msg->id) {
        default:
            ESP_LOGD(TAG, "[%s] Unhandled message, passing to general handler",
                    el->tag);
            return audio_element_msg_handler(el, msg);
    }

    return ESP_OK;
}


audio_element_t *sdcard_stream_init(sdcard_stream_cfg_t *config) {
    sdcard_stream_t *stream = calloc(1, sizeof(sdcard_stream_t));
    if (!stream) {
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

    stream->type = config->type;
    if (config->type == AEL_STREAM_READER) {
        cfg.read = _sdcard_read;
    } else {
        // TODO: Add support for file writing
        // AEL_STREAM_WRITER;
        ESP_LOGE(TAG, "AEL_STREAM_WRITER Not implemented yet.");
        return NULL;
    }

    audio_element_t *el = audio_element_init(&cfg);
    if (!el) {
        ESP_LOGE(TAG, "[%s] Could not allocate memory for audio element.",
                cfg.tag);
    }
    el->data = stream;

    return el;
}
