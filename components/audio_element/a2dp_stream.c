#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "audio_element.h"
#include "a2dp_stream.h"
#include "io.h"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"

#include <string.h>

static const char TAG[] = "A2DP_STREAM";

static const char *s_a2d_conn_state_str[] =
    {"Disconnected", "Connecting","Connected", "Disconnecting"};
static const char *s_a2d_audio_state_str[] =
    {"Suspended", "Stopped", "Started"};


typedef void (* evt_hdl_fn_t)(audio_element_t *el, uint16_t event,
        void *param);
typedef struct {
    uint16_t        event;
    evt_hdl_fn_t    func;
    void            *param;
} msg_t;

typedef struct {
    audio_stream_type_t type;
    QueueHandle_t       msg_queue;
} a2dp_stream_t;


static QueueHandle_t   gs_msg_queue;
static RingbufHandle_t gs_rb;


/**
 * BT A2DP functions
 **/

// Helper functions

static inline bool send_msg(msg_t *msg) {
    if (msg == NULL)
        return false;

    if (xQueueSend(gs_msg_queue, msg, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGE(TAG, "xQueueSend error");
        return false;
    }

    return true;
}

// Send work to a2dp task to do
static bool work_dispatch(evt_hdl_fn_t func, uint16_t event, void *param, int param_len) {
    ESP_LOGD(TAG, "Dispatch event 0x%x, param len %d", event, param_len);

    msg_t msg;
    msg.event = event;
    msg.func = func;

    if (param_len == 0) {
        return send_msg(&msg);
    } else if (param && param_len > 0) {
        if ((msg.param = malloc(param_len)) != NULL) {
            memcpy(msg.param, param, param_len);
            return send_msg(&msg);
        }
    }

    return false;
}

// Secondary handlers

void bt_hdl_notify_evt(esp_avrc_rn_event_ids_t event,
        esp_avrc_rn_param_t *param) {
    switch(event) {
        case ESP_AVRC_RN_PLAY_STATUS_CHANGE:
        {
            ESP_LOGI(TAG, "Remote playback status changed: %d",
                    param->playback);
            break;
        }

        case ESP_AVRC_RN_TRACK_CHANGE:
        {
            ESP_LOGI(TAG, "Track changed notification");
            break;
        }

        case ESP_AVRC_RN_PLAY_POS_CHANGED:
        {
            ESP_LOGI(TAG, "Play position changed: %d", param->play_pos);
            break;
        }

        case ESP_AVRC_RN_VOLUME_CHANGE:
        {
            ESP_LOGI(TAG, "Volume changed: %d", param->play_pos);
            break;
        }

        default:
            ESP_LOGW(TAG, "Unhandled NOTIFY event: %d", event);
            break;
    }
}

// Primary handlers

void bt_hdl_a2d_evt(audio_element_t *el, uint16_t event, void *param) {
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *) param;
    audio_element_info_t *info = &el->info;
    ESP_LOGD(TAG, "a2d_evt received: 0x%x", event);

    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = a2d->conn_stat.remote_bda;
            ESP_LOGI(TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_a2d_conn_state_str[a2d->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
            if (a2d->conn_stat.state ==
                    ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                        ESP_BT_GENERAL_DISCOVERABLE);

            } else if (a2d->conn_stat.state ==
                    ESP_A2D_CONNECTION_STATE_CONNECTED){
                esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,
                        ESP_BT_NON_DISCOVERABLE);

            }
            break;
        }

        case ESP_A2D_AUDIO_CFG_EVT:
        {
            ESP_LOGD(TAG, "Audio Codec configured:");

            if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
                uint32_t sample_rate = 16000;

                // Retireve current sample rate 
                uint8_t oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
                if (oct0 & (1 << 6))
                    sample_rate = 32000;
                else if (oct0 & (1 << 5))
                    sample_rate = 44100;
                else if (oct0 & (1 << 4))
                    sample_rate = 48000;
                info->sample_rate = sample_rate;

                // Retrieve current number of channels
                uint8_t channels = 2;
                if (oct0 & (1 << 3))
                    channels = 1;
                info->channels = channels;

                // Retrieve current bits per second
                uint16_t bits_per_sample = 8;
                uint8_t oct1 = a2d->audio_cfg.mcc.cie.sbc[1];
                if (oct1 & (1 << 5) || oct1 & (1 << 4))
                    bits_per_sample = 16;
                info->bps = bits_per_sample;

                ESP_LOGI(TAG, "Received configuration: Samplerate %d, bps %d", sample_rate, bits_per_sample);
            }
            break;
        }

        case ESP_A2D_AUDIO_STATE_EVT:
        {
            ESP_LOGI(TAG, "A2DP Audio state: %s",
                    s_a2d_audio_state_str[a2d->audio_stat.state]);
            break;
        }

        default:
            ESP_LOGW(TAG, "[%s] unhandled event %d", el->tag, event);
            break;
    }

    /* switch (event) { */
    /*     case ESP_A2D_AUDIO_STATE_EVT: { */
    /*         ESP_LOGI(TAG, "A2DP audio state: %s", s_a2d_audio_state_str[a2d->audio_stat.state]); */
    /*         switch(a2d->audio_stat.state) { */
    /*             case ESP_A2D_AUDIO_STATE_STARTED: */
    /*                 /1* s_state->status = PLAYING; *1/ */
    /*                 break; */
    /*             case ESP_A2D_AUDIO_STATE_STOPPED: */
    /*                 /1* s_state->status = STOPPED; *1/ */
    /*                 break; */
    /*             case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND: */
    /*                 /1* s_state->status = PAUSED; *1/ */
    /*                 break; */
    /*             default: */
    /*                 ESP_LOGE(TAG, "Unimplemented Audio State EVT"); */
    /*                 break; */
    /*         } */
    /*         break; */
    /*     } */
    /* } */
}

// Handle received commands from controller
static void bt_hdl_rc_tg_evt(audio_element_t *el, uint16_t event,
        void *param) {
    esp_avrc_tg_cb_param_t *rc = (esp_avrc_tg_cb_param_t *) param;

    switch(event) {
        case ESP_AVRC_TG_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = rc->conn_stat.remote_bda;
            ESP_LOGI(TAG, "AVRC tg conn_state evt: %s, addr[%02x:%02x:%02x:%02x:%02x:%02x]",
                    s_a2d_conn_state_str[rc->conn_stat.connected],
                    bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
            break;
        }

        case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
        {
            ESP_LOGI(TAG, "AVRC register event notification: 0x%x, param: 0x%x", 
                    rc->reg_ntf.event_id, rc->reg_ntf.event_parameter);
            break;
        }

        case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
        {
            ESP_LOGI(TAG, "Set absolute volume cmd received. val: %d",
                    rc->set_abs_vol.volume);
            break;
        }

        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
    }
}

static void bt_hdl_rc_ct_evt(audio_element_t *el, uint16_t event,
        void *param) {
    ESP_LOGD(TAG, "%s avrc controller evt %d", __func__, event);
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *) (param);

    switch (event) {
        case ESP_AVRC_TG_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = rc->conn_stat.remote_bda;
            ESP_LOGI(TAG, "AVRC ct conn_state evt: %s, addr[%02x:%02x:%02x:%02x:%02x:%02x]",
                    s_a2d_conn_state_str[rc->conn_stat.connected],
                    bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
            break;
        }

        case ESP_AVRC_CT_METADATA_RSP_EVT:
        {
            ESP_LOGI(TAG, "AVRC Metadata RSP: attr id 0x%x, %s",
                    rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
            free(rc->meta_rsp.attr_text);
            break;
        }

        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
        {
            ESP_LOGD(TAG, "%s Received CHANGE_NOTIFY_EVT %d", __func__,
                    rc->change_ntf.event_id);

            bt_hdl_notify_evt(rc->change_ntf.event_id,
                    &rc->change_ntf.event_parameter);
            break;
        }

        default:
            ESP_LOGW(TAG, "Unhandled ct_evt: 0x%x", event);
            break;
    }
}


// Callbacks

static void bt_gap_cb(esp_bt_gap_cb_event_t event,
        esp_bt_gap_cb_param_t *param) {
    
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Auth success with: %s",
                        param->auth_cmpl.device_name);
                esp_log_buffer_hex(TAG, param->auth_cmpl.bda,
                        ESP_BD_ADDR_LEN);
            } else {
                ESP_LOGE(TAG, "Auth failed, status: %d",
                        param->auth_cmpl.stat);
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unhandled GAP event: %d", event);
            break;
    }
}

static void bt_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
        case ESP_A2D_AUDIO_STATE_EVT:
        case ESP_A2D_AUDIO_CFG_EVT:
            work_dispatch(bt_hdl_a2d_evt, event, param,
                    sizeof(esp_a2d_cb_param_t));
            break;
        default:
            ESP_LOGE(TAG, "Unimplemented A2DP event %d", event);
            break;
    }
}

static void bt_a2d_data_cb(const uint8_t *data, uint32_t len) {
    ESP_LOGV(TAG, "[a2dp] Data received: %d bytes", len);

    xRingbufferSend(gs_rb, data, len, portMAX_DELAY);
}

static void bt_rc_ct_cb(esp_avrc_ct_cb_event_t event,
        esp_avrc_ct_cb_param_t *param) {
    // TODO: actual implementation
    
    work_dispatch(bt_hdl_rc_ct_evt, event, param,
            sizeof(esp_avrc_ct_cb_param_t));
}

static void bt_rc_tg_cb(esp_avrc_tg_cb_event_t event,
        esp_avrc_tg_cb_param_t *param) {
    // TODO: actual implementation
    
    work_dispatch(bt_hdl_rc_tg_evt, event, param,
            sizeof(esp_avrc_tg_cb_param_t));
}

static void bt_init() {
    ESP_LOGI(TAG, "Initializing Bluetooth");

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // TODO: Clean this up and do some proper error checking
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // TODO: Better pin handling, probably SSP
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code = { '1', '2', '3', '4' };
    esp_bt_gap_set_pin(pin_type, 4, pin_code);
}


/**
 * Stream functions
 **/

static esp_err_t _a2dp_open(audio_element_t *el, void* pv) {
    /* audio_element_info_t info = el->info; */
    a2dp_stream_t *stream = el->data;
    char *device_name = pv;

    // Set global msg_queue and ringbuf to this elements'
    // This is needed so that the bluetooth callback functions can
    // access this queue
    gs_msg_queue = stream->msg_queue;
    gs_rb = el->output->rb;
    
    // Set the device name
    ESP_LOGI(TAG, "[%s] Setting device name to %s", el->tag, device_name);
    esp_bt_dev_set_device_name(device_name);

    // GAP callback 
    esp_bt_gap_register_callback(bt_gap_cb);

    // AVRCP Controller callback
    // (Received response from controller)
    ESP_ERROR_CHECK(esp_avrc_ct_init());
    esp_avrc_ct_register_callback(bt_rc_ct_cb);
    // AVRCP Target callback
    // (Received cmd from controller)
    ESP_ERROR_CHECK(esp_avrc_tg_init());
    esp_avrc_tg_register_callback(bt_rc_tg_cb);

    esp_avrc_rn_evt_cap_mask_t evt_set = {0};

    esp_avrc_tg_get_rn_evt_cap(ESP_AVRC_RN_CAP_ALLOWED_EVT, &evt_set);
    ESP_LOGI(TAG, "supported cap: 0x%x", evt_set.bits);

    esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
    /* esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_PLAY_STATUS_CHANGE); */
    /* esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_PLAY_POS_CHANGED); */
    ESP_ERROR_CHECK(esp_avrc_tg_set_rn_evt_cap(&evt_set));


    // A2DP Sink
    // Handles a2d connection, stream state and codec configuration
    esp_a2d_register_callback(bt_a2d_cb);
    // Handles actual data received
    esp_a2d_sink_register_data_callback(bt_a2d_data_cb);
    esp_a2d_sink_init();

    // Enable discovery, and contectability
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
            ESP_BT_GENERAL_DISCOVERABLE);

    
    el->is_open = true;

    return ESP_OK;
}


//TODO
static esp_err_t _a2dp_close(audio_element_t *el) {
    return ESP_OK;
}


//TODO
static esp_err_t _a2dp_destroy(audio_element_t *el) {
    /* a2dp_stream_t *stream = el->data; */

    return ESP_OK;
}

static size_t _a2dp_process(audio_element_t *el) {
    static msg_t msg;
    /* static a2dp_stream_t *stream = el->data; */

    if (xQueueReceive(gs_msg_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGD(TAG, "[%s] Received a msg to handle", el->tag);

        if (msg.func)
            msg.func(el, msg.event, msg.param);
        
        if (msg.param)
            free(msg.param);
    }
    return 0;
}


/*
static size_t _a2dp_read(io_t *io, char *buf, size_t len, void *pv) {
    a2dp_stream_t *stream = ((audio_element_t *)pv)->data;

    size_t bytes_written;
    i2s_write(stream->i2s_num, buf, len, &bytes_written, portMAX_DELAY);

    ESP_LOGD(TAG, "Bytes read to a2dp_ %d", bytes_written);

    return bytes_written;
}
*/


audio_element_t *a2dp_stream_init(audio_element_cfg_t cfg, audio_stream_type_t type) {
    ESP_LOGI(TAG, "[a2dp] Initializing");
    a2dp_stream_t *stream = calloc(1, sizeof(a2dp_stream_t));
    if (!stream) {
        ESP_LOGE(TAG, "Could not allocate memory!");
        return NULL;
    }

    stream->msg_queue = xQueueCreate(10, sizeof(msg_t));

    // Configure audio_element_t
    cfg.open = _a2dp_open;
    cfg.close = _a2dp_close;
    cfg.process = _a2dp_process;
    cfg.destroy = _a2dp_destroy;

    cfg.tag = "a2dp";

    stream->type = type;
    if (type == AEL_STREAM_READER) {
        /* cfg.read = _a2dp_read; */
    } else {
        ESP_LOGE(TAG, "AEL_STREAM_WRITER Not implemented.");
        return NULL;
    }

    // Input is not used, as the output RB is being written by a cb
    cfg.input = (io_t *)1;

    audio_element_t *el = audio_element_init(&cfg);
    if (!el) {
        ESP_LOGE(TAG, "[%s] could not init audio element.", el->tag);
        return NULL;
    }
    el->data = stream;

    ESP_LOGI(TAG, "[%s] Initializing bluetooth controller", el->tag);
    bt_init();

    ESP_LOGI(TAG, "[%s] Done.", el->tag);

    return el;
}
