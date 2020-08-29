#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "audio_element.h"

#include "esp_err.h"
#include "esp_log.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define DEFAULT_TICKS_TO_WAIT pdMS_TO_TICKS(500)
#define DEFAULT_MSG_QUEUE_LENGTH 16

enum notification_bit {
    AEL_BIT_STATUS_CHANGED = 1,  // Set but unused for now
};


const char *audio_element_status_str[] = {
    "PLAYING", "PAUSED", "WAITING", "STOPPED"
};

static const char TAG[] = "AEL";


static size_t _default_process(audio_element_t *el) {
    ESP_LOGV(TAG, "[%s] Default process running", el->tag);
    audio_element_info_t *input_info = el->input->user_data;

    if (input_info->changed) {
        ESP_LOGD(TAG, "[%s] input info changed, updating output info",
                el->tag);
        audio_element_set_info(el->output, audio_element_get_info(el->input));
        input_info->changed = false;
    }

    size_t bytes_read = el->input->read(el->input, el->buf, el->buf_len, el);
    if (bytes_read > 0) {
        return el->output->write(el->output, el->buf, el->buf_len, el);
    }
    return 0;
}


static void audio_element_destroy(audio_element_t *el) {
    audio_element_info_t *info;

    if (el->destroy)
        el->destroy(el);

    free(el->buf);

    info = el->input->user_data;
    vSemaphoreDelete(info->lock);
    io_destroy(el->input);

    info = el->output->user_data;
    vSemaphoreDelete(info->lock);
    io_destroy(el->output);

    free(el);
    el = NULL;
}


static io_t *audio_element_io_create(io_cb read, io_cb write, int size) {
    io_t *buf = io_create(read, write, size);
    if (!buf) {
        return NULL;
    }
    
    buf->user_data = calloc(1, sizeof(audio_element_info_t));
    if (!buf->user_data) {
        return NULL;
    }

    // Create mutex, and set info to default values
    audio_element_info_t *info = buf->user_data;
    info->lock = xSemaphoreCreateMutex();
    // TODO: Make some more general way of setting these defaults
    info->sample_rate = 44100;
    info->channels = 2;
    info->bits = 16;

    return buf;
}


void audio_element_task(void *pv) {
    audio_element_t *el = (audio_element_t*) pv;
    int process_res;
    BaseType_t notify_res;
    uint32_t notification = 0;

    el->status = AEL_STATUS_PLAYING;
    el->task_running = true;
    while (el->task_running) {

        // Check for notification
        notify_res = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notification, 0);
        if (notify_res == pdTRUE) {
            ESP_LOGD(TAG, "[%s] Notification received! 0x%x", el->tag,
                    notification);

            // TODO: Change status
        }


        // Process audio data
        if (el->is_open) {
            process_res = el->process(el);
            if (process_res < 0) {
                // Some error, print debug info
                // -2 is when no data was written (e.g. buffer full)
                switch (process_res) {
                    case IO_WRITE_ERROR:
                        ESP_LOGW(TAG, "[%s] Could not write to output",
                                el->tag);
                        break;
                    default:
                    ESP_LOGW(TAG, "[%s] Process returned error: %d, %s",
                            el->tag, process_res,
                            esp_err_to_name(process_res));
                }
            }
        } else {
            ESP_LOGD(TAG, "[%s] Not yet open", el->tag);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGD(TAG, "[%s] Closing", el->tag);
    if (el->is_open && el->close) {
        el->close(el);
    }
    el->is_open = false;

    audio_element_destroy(el);

    ESP_LOGI(TAG, "[%s] task deleted. Max mem usage: %d", el->tag,
            uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}


audio_element_t *audio_element_init(audio_element_cfg_t *config) {
    audio_element_t *el = calloc(1, sizeof(audio_element_t));
    if (el == NULL) {
        ESP_LOGE(TAG, "Could not allocate memory for element");
        return NULL;
    }

    // Allocate memory for working buffer, if needed
    if (config->buf_len) {
        el->buf = malloc(config->buf_len);
        if (el->buf == NULL) {
            ESP_LOGE(TAG, "[%s] Could not allocate memory for internal buffer",
                    config->tag);
            return NULL;
        }
    }
    el->buf_len = config->buf_len;

    // Set callback functions
    el->open = config->open;
    el->close = config->close;
    el->destroy = config->destroy;

    if (config->process)
        el->process = config->process;
    else
        el->process = _default_process;

    // If read callback function provided, configure input callback
    // otherwise configure input ringbuffer
    if (config->input)
        el->input = config->input;
    else {
        el->input = audio_element_io_create(config->read, config->write, 0);
        if (!el->input) {
            ESP_LOGE(TAG, "Could not create input buffer");
            return NULL;
        }
    }
    
    // If write callback function provided, configure output callback
    // otherwise configure output ringbuffer
    if (config->output)
        el->output = config->output;
    else {
        el->output = audio_element_io_create(config->read, config->write,
                config->out_rb_size);
        if (!el->output) {
            ESP_LOGE(TAG, "Could not create output buffer");
            return NULL;
        }
    }

    el->tag = config->tag;
    el->status = AEL_STATUS_STOPPED;
    
    // Create task if needed
    if (config->task_stack > 0) {
        xTaskCreate(audio_element_task, config->tag, config->task_stack, el,
                configMAX_PRIORITIES, &el->task_handle);
    }

    el->is_open = false;
    return el;
}


void audio_element_cfg_link(audio_element_t *from, audio_element_cfg_t *to) {
    to->input = from->output;
}


void audio_element_cfg_clear(audio_element_cfg_t *cfg) {
    audio_element_cfg_t def = DEFAULT_AUDIO_ELEMENT_CFG();
    memset(cfg, 0, sizeof(audio_element_cfg_t));
    memcpy(cfg, &def, sizeof(audio_element_cfg_t));
}


esp_err_t audio_element_notify(audio_element_t *el, int bits) {
    if (xTaskNotify(el->task_handle, bits, eSetBits) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_FAIL;
}


// TODO: Implement this
void audio_element_change_status(audio_element_t *el,
        audio_element_status_t status) {
    ESP_LOGI(TAG, "[%s] Setting status to: %s", el->tag,
            audio_element_status_str[el->status]);
    el->status = status;
    /* if (audio_element_notify(el, AEL_BIT_STATUS_CHANGED)) */
    /*     el->status = status; */
}


void audio_element_set_info(io_t *io, audio_element_info_t new_info) {
    ESP_LOGD(TAG, "Setting a new info");
    audio_element_info_t *info = io->user_data;

    xSemaphoreTake(info->lock, portMAX_DELAY);
    info->sample_rate = new_info.sample_rate;
    info->channels = new_info.changed;
    info->bits = new_info.bits;
    info->changed = true;
    xSemaphoreGive(info->lock);
}


audio_element_info_t audio_element_get_info(io_t *io) {
    ESP_LOGD(TAG, "Getting info from io_t");
    audio_element_info_t *info = io->user_data;
    audio_element_info_t out_info;

    xSemaphoreTake(info->lock, portMAX_DELAY);
    memcpy(&out_info, info, sizeof(audio_element_info_t));
    xSemaphoreGive(info->lock);

    return out_info;
}
