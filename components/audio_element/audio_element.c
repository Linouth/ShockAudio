#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "audio_element.h"

#include "esp_err.h"
#include "esp_log.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define DEFAULT_TICKS_TO_WAIT portMAX_DELAY
#define DEFAULT_MSG_QUEUE_LENGTH 16

static const char TAG[] = "AEL";


static esp_err_t audio_element_msg_handler(audio_element_t *el, msg_t *msg) {
    ESP_LOGD(TAG, "[%s] msg received: %d, data_len: %d", el->tag, msg->id,
            msg->data_len);

    switch(msg->id) {
        case AEL_MSG_STOP:
            ESP_LOGI(TAG, "[%s] Stopping audio element.", el->tag);
            el->task_running = false;
            break;
        default:
            ESP_LOGW(TAG, "[%s] Unhandled msg: %d", el->tag, msg->id);
            return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}


esp_err_t audio_element_msg_send(audio_element_t *el, msg_t *msg) {
    ESP_LOGD(TAG, "[%s] Sending message %d", el->tag, msg->id);
    if (xQueueSendToBack(el->msg_queue, msg, DEFAULT_TICKS_TO_WAIT) !=
            pdTRUE) {
        ESP_LOGE(TAG, "[%s] Message queue fulL!", el->tag);
        return ESP_FAIL;
    }
    return ESP_OK;
}


void audio_element_destory(audio_element_t *el) {
    free(el->buf);

    if (el->input.type == IO_TYPE_RB && el->input.data.rb)
        vRingbufferDelete(el->input.data.rb);

    if (el->output.type == IO_TYPE_RB && el->output.data.rb)
        vRingbufferDelete(el->output.data.rb);

    vQueueDelete(el->msg_queue);
    free(el);
    el = NULL;
}


void audio_element_task(void *pv) {
    audio_element_t *el = (audio_element_t*) pv;
    msg_t msg;
    esp_err_t err;

    el->task_running = true;
    while (el->task_running) {
        if ((xQueueReceive(el->msg_queue, &msg, 0)) == pdTRUE) {
            // Message received!
            
            // Handle message
            el->msg_handler(el, &msg);
        }

        // Process audio data
        err = el->process(el);
        if (err != ESP_OK) {
            // Some error, print debug info
            ESP_LOGW(TAG, "[%s] Process returned error: %d, %s", el->tag, err,
                    esp_err_to_name(err));
        }
    }

    if (el->is_open && el->close) {
        ESP_LOGD(TAG, "[%s] Closing", el->tag);
        el->close(el);
    }
    el->is_open = false;

    ESP_LOGI(TAG, "[%s] task deleted. Max mem usage: %d", el->tag,
            uxTaskGetStackHighWaterMark(NULL));
    audio_element_destory(el);
    vTaskDelete(NULL);
}


audio_element_t *audio_element_init(audio_element_cfg_t *config) {
    audio_element_t *el = calloc(1, sizeof(audio_element_t));
    if (el == NULL) {
        ESP_LOGE(TAG, "Could not allocate memory for element");
        return NULL;
    }

    el->buf = malloc(config->buf_size);
    if (el->buf == NULL) {
        ESP_LOGE(TAG, "[%s] Could not allocate memory for internal buffer",
                config->tag);
        return NULL;
    }

    // Set callback functions
    el->open = config->open;
    el->close = config->close;
    el->destroy = config->destroy;
    el->process = config->process;

    // Use custom MSG callback function if provided
    if (config->msg_handler)
        el->msg_handler = config->msg_handler;
    else
        el->msg_handler = audio_element_msg_handler;

    // If read callback function provided, configure input callback
    // otherwise configure input ringbuffer
    if (config->read) {
        el->input.type = IO_TYPE_CB;
        el->input.data.func = config->read;
        el->input.ready = true;
    } else {
        el->input.type = IO_TYPE_RB;
        el->input.ready = false;
    }

    // If write callback function provided, configure output callback
    // otherwise configure output ringbuffer
    if (config->write) {
        el->output.type = IO_TYPE_CB;
        el->output.data.func = config->write;
        el->output.ready = true;
    } else {
        el->output.type = IO_TYPE_RB;
        el->output.ready = true;

        if (!config->out_rb_size) {
            ESP_LOGW(TAG, "[%s] No output ringbuffer size specified. Using %d",
                    config->tag, DEFAULT_OUT_RB_SIZE);
            config->out_rb_size = DEFAULT_OUT_RB_SIZE;
        }
        el->output.data.rb = xRingbufferCreate(config->out_rb_size,
                RINGBUF_TYPE_BYTEBUF);
    }

    el->tag = config->tag;

    // Configure audio element data information
    audio_element_info_t info = DEFAULT_AUDIO_ELEMENT_INFO();
    el->info = info;
    
    // Create task if needed
    if (config->task_stack > 0) {
        xTaskCreate(audio_element_task, config->tag, config->task_stack, el,
                configMAX_PRIORITIES, &el->task_handle);

        // Create message queue to use in the task
        el->msg_queue = xQueueCreate(DEFAULT_MSG_QUEUE_LENGTH, sizeof(msg_t));
    }

    return el;
}


size_t audio_element_input(audio_element_t *el, char *buf, size_t len) {
    size_t bytes_read = 0;

    if (!el->input.ready) {
        ESP_LOGE(TAG, "[%s] Input is not ready yet. Make sure to link an"
                "output ringbuf to the input ringbuf", el->tag);
        return ESP_FAIL;
    }

    // Receive input data from callback or from ringbuffer
    if (el->input.type == IO_TYPE_CB) {
        bytes_read = el->input.data.func(el, buf, len);
    } else if (el->input.type == IO_TYPE_RB) {
        void *data = xRingbufferReceiveUpTo(el->input.data.rb, &bytes_read,
                DEFAULT_TICKS_TO_WAIT, len);
        memcpy(buf, data, bytes_read);
        vRingbufferReturnItem(el->input.data.rb, data);
    } else {
        ESP_LOGE(TAG, "[%s] Invalid IO_TYPE: %d", el->tag, el->input.type);
        return ESP_FAIL;
    }
    
    return bytes_read;
}


size_t audio_element_output(audio_element_t *el, char *buf, size_t len) {
    size_t bytes_written = 0;

    if (!el->output.ready) {
        ESP_LOGE(TAG, "[%s] Output is not ready yet.", el->tag);
        return ESP_FAIL;
    }

    // Write output data using callback or to ringbuffer
    if (el->output.type == IO_TYPE_CB) {
        bytes_written = el->output.data.func(el, buf, len);
    } else if (el->output.type == IO_TYPE_RB) {
        BaseType_t ret = xRingbufferSend(el->output.data.rb, buf, len,
                DEFAULT_TICKS_TO_WAIT);
        if (ret == pdTRUE)
            bytes_written = len;
        else
            bytes_written = -1;
    } else {
        ESP_LOGE(TAG, "[%s] Invalid IO_TYPE: %d", el->tag, el->output.type);
        return ESP_FAIL;
    }

    return bytes_written;
}
