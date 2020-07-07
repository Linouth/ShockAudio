#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "audio_element.h"

#include "esp_err.h"
#include "esp_log.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define DEFAULT_TICKS_TO_WAIT pdMS_TO_TICKS(500)
#define DEFAULT_MSG_QUEUE_LENGTH 16

enum notification_bit {
    AEL_BIT_MSG            = 1,
    AEL_BIT_STATUS_CHANGED = 2,  // Set but unused for now
};


const char *audio_element_status_str[] = {
    "PLAYING", "PAUSED", "WAITING", "STOPPED"
};

static const char TAG[] = "AEL";


audio_element_msg_t audio_element_msg_create(audio_element_msg_id_t id,
        size_t size) {
    audio_element_msg_t msg = { 0 };
    msg.id = id;
    msg.data_len = size;
    if (size > 0)
        msg.data = malloc(size);

    return msg;
}

esp_err_t audio_element_msg_handler(audio_element_t *el,
        audio_element_msg_t *msg) {
    ESP_LOGD(TAG, "[%s] msg received: %d, data_len: %d", el->tag, msg->id,
            msg->data_len);

    switch(msg->id) {
        case AEL_MSG_STATUS_CHANGE:
            // Change the AEL status
            el->status = (audio_element_status_t)*msg->data;
            audio_element_notify(el, AEL_BIT_STATUS_CHANGED);
            ESP_LOGI(TAG, "[%s] Changing status to %s", el->tag,
                    audio_element_status_str[el->status]);
            break;
        case AEL_MSG_STOP:
            ESP_LOGI(TAG, "[%s] Stopping audio element.", el->tag);
            el->status = AEL_STATUS_STOPPED;
            el->task_running = false;
            break;
        default:
            ESP_LOGW(TAG, "[%s] Unhandled msg: %d", el->tag, msg->id);
            return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}


static void audio_element_destory(audio_element_t *el) {
    if (el->destroy)
        el->destroy(el);

    free(el->buf);

    if (el->input.type == IO_TYPE_RB && el->input.data.rb)
        vRingbufferDelete(el->input.data.rb);

    if (el->output.type == IO_TYPE_RB && el->output.data.rb)
        vRingbufferDelete(el->output.data.rb);

    vQueueDelete(el->msg_queue);
    free(el);
    el = NULL;
}


esp_err_t audio_element_status_set(
        audio_element_t *el, audio_element_status_t status) {

    if (el->status == status) {
        ESP_LOGE(TAG, "[%s] AEL already has status %s", el->tag,
                audio_element_status_str[status]);
        return ESP_FAIL;
    }

    // Create msg struct
    audio_element_msg_t msg = audio_element_msg_create(
            AEL_MSG_STATUS_CHANGE, sizeof(audio_element_status_t));
    *msg.data = status;

    // Send message
    return audio_element_msg_send(el, &msg);
}


void audio_element_task(void *pv) {
    audio_element_t *el = (audio_element_t*) pv;
    audio_element_msg_t msg;
    int process_res;
    BaseType_t notify_res;
    uint32_t notification = 0;

    TickType_t maxBlockTime = pdMS_TO_TICKS(200);

    el->task_running = true;
    while (el->task_running) {

        if (el->wait_for_notify) {
            notify_res = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notification,
                    portMAX_DELAY);

            if (notify_res == pdTRUE) {
                ESP_LOGD(TAG, "[%s] Notification received! 0x%x", el->tag,
                        notification);

                // Check if message was sent
                if (notification & AEL_BIT_MSG) {
                    if ((xQueueReceive(el->msg_queue, &msg, maxBlockTime))
                            == pdTRUE) {
                        // Message received!
                        
                        // Handle message
                        el->msg_handler(el, &msg);

                        // Free msg data if provided
                        if (msg.data)
                            free(msg.data);
                    }
                }

                if (el->status == AEL_STATUS_WAITING ||
                        el->status == AEL_STATUS_PAUSED)
                    // Don't reset waiting_for_notify, so task will wait for
                    // other notifications until status has changed
                    continue;

                el->wait_for_notify = false;
            } else 
                ESP_LOGW(TAG, "[%s] wait_for_notify == true, "
                        "but no notification received", el->tag);
        }


        // Process audio data
        if (el->is_open) {
            process_res = el->process(el);
            if (process_res < 0) {
                // Some error, print debug info
                // -2 is when no data was written (e.g. buffer full)
                switch (process_res) {
                    case -2:
                        ESP_LOGW(TAG, "[%s] Could not write to output",
                                el->tag);
                        break;
                    default:
                    ESP_LOGW(TAG, "[%s] Process returned error: %d, %s",
                            el->tag, process_res,
                            esp_err_to_name(process_res));
                }
            }
        }
    }

    if (el->is_open && el->close) {
        ESP_LOGD(TAG, "[%s] Closing", el->tag);
        el->close(el);
    }
    el->is_open = false;

    audio_element_destory(el);

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

    el->buf = malloc(config->buf_size);
    if (el->buf == NULL) {
        ESP_LOGE(TAG, "[%s] Could not allocate memory for internal buffer",
                config->tag);
        return NULL;
    }
    el->buf_size = config->buf_size;

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
        el->msg_queue = xQueueCreate(DEFAULT_MSG_QUEUE_LENGTH,
                sizeof(audio_element_msg_t));
    }

    el->is_open = false;
    el->wait_for_notify = false;

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

    ESP_LOGD(TAG, "Read %d bytes", bytes_read);
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
            bytes_written = -2;
    } else {
        ESP_LOGE(TAG, "[%s] Invalid IO_TYPE: %d", el->tag, el->output.type);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Written %d bytes", bytes_written);
    return bytes_written;
}


esp_err_t audio_element_notify(audio_element_t *el, int bits) {
    if (xTaskNotify(el->task_handle, bits, eSetBits) == pdTRUE) {
        el->wait_for_notify = true;
        return ESP_OK;
    }
    return ESP_FAIL;
}


esp_err_t audio_element_msg_send(audio_element_t *el,
        audio_element_msg_t *msg) {
    ESP_LOGD(TAG, "[%s] Sending message %d", el->tag, msg->id);
    if (xQueueSendToBack(el->msg_queue, msg, DEFAULT_TICKS_TO_WAIT)
            == pdTRUE) {
        return audio_element_notify(el, AEL_BIT_MSG);
    }

    ESP_LOGE(TAG, "[%s] Message queue full!", el->tag);
    return ESP_FAIL;
}


void audio_element_play(audio_element_t *el) {
    audio_element_status_set(el, AEL_STATUS_PLAYING);
}


void audio_element_pause(audio_element_t *el) {
    audio_element_status_set(el, AEL_STATUS_PAUSED);
}
