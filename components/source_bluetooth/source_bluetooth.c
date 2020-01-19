#include "source_bluetooth.h"
#include "audio_source.h"
#include "audio_buffer.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"

typedef void (* cb_t)(uint16_t event, void *param);

typedef struct {
    uint16_t signal;
    uint16_t event;
    cb_t     cb;
    void     *param;
} msg_t;


enum {
    BT_SIG_WORK_DISPATCH = 0
};

enum {
    BT_EVT_STACK_UP = 0,
};

enum {
    RC_CT_TL_GET_CAPS = 0,
};


static const char* TAG = "Bluetooth";

static const char *s_a2d_conn_state_str[] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
static const char *s_a2d_audio_state_str[] = {"Suspended", "Stopped", "Started"};

static esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;

static xQueueHandle s_bt_task_queue = NULL;
static xTaskHandle s_bt_task_handle = NULL;
static source_state_t *s_state;


bool send_msg(msg_t *msg) {
    if (msg == NULL)
        return false;

    if (xQueueSend(s_bt_task_queue, msg, 10 / portTICK_RATE_MS) != pdTRUE) {
        ESP_LOGE(TAG, "%s xQueueSend error", __func__);
        return false;
    }

    return true;
}


bool work_dispatch(cb_t cb, uint16_t event, void *params, int param_len) {
    ESP_LOGD(TAG, "%s event 0x%x, param len %d", __func__, event, param_len);

    msg_t msg;
    memset(&msg, 0, sizeof(msg_t));

    msg.signal = BT_SIG_WORK_DISPATCH;
    msg.event = event;
    msg.cb = cb;
    
    if (param_len == 0) {
        // Send
        return send_msg(&msg);
    } else if (params && param_len > 0) {
        if ((msg.param = malloc(param_len)) != NULL) {
            memcpy(msg.param, params, param_len);
            return send_msg(&msg);
        } else {
            ESP_LOGE(TAG, "Could not allocate memory for msg.param");
        }
    }

    return false;
}


void bt_init() {
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


void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Auth success with: %s", param->auth_cmpl.device_name);
                esp_log_buffer_hex(TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
            } else {
                ESP_LOGE(TAG, "Auth failed, status: %d", param->auth_cmpl.stat);
            }
        }
        break;
    default:
        ESP_LOGW(TAG, "Unhandled GAP event: %d", event);
        break;
    }
}


void bt_hdl_a2d_evt(uint16_t event, void *param) {
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *) param;
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            uint8_t *bda = a2d->conn_stat.remote_bda;
            ESP_LOGI(TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_a2d_conn_state_str[a2d->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
            if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
                s_state->status = STOPPED;
            } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED){
                esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
                s_state->status = PLAYING;
            }
            break;
        }
        case ESP_A2D_AUDIO_STATE_EVT: {
            ESP_LOGI(TAG, "A2DP audio state: %s", s_a2d_audio_state_str[a2d->audio_stat.state]);
            switch(a2d->audio_stat.state) {
                case ESP_A2D_AUDIO_STATE_STARTED:
                    s_state->status = PLAYING;
                    break;
                case ESP_A2D_AUDIO_STATE_STOPPED:
                    s_state->status = STOPPED;
                    break;
                case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
                    s_state->status = PAUSED;
                    break;
                default:
                    ESP_LOGE(TAG, "Unimplemented Audio State EVT");
                    break;
            }
            break;
        }
        case (ESP_A2D_AUDIO_CFG_EVT):
            {
                ESP_LOGD(TAG, "Audio Codec configured:");
                if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
                    uint32_t sample_rate = 16000;

                    uint8_t oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
                    if (oct0 & (1 << 6))
                        sample_rate = 32000;
                    else if (oct0 & (1 << 5))
                        sample_rate = 44100;
                    else if (oct0 & (1 << 4))
                        sample_rate = 48000;
                    s_state->buffer.format.sample_rate = sample_rate;


                    uint8_t channels = 2;
                    if (oct0 & (1 << 3))
                        channels = 1;
                    s_state->buffer.format.channels = channels;


                    uint16_t bits_per_sample = 8;
                    uint8_t oct1 = a2d->audio_cfg.mcc.cie.sbc[1];
                    if (oct1 & (1 << 5) || oct1 & (1 << 4))
                        bits_per_sample = 16;

                    s_state->buffer.format.bits_per_sample = bits_per_sample;

                    ESP_LOGI(TAG, "Received configuration: Samplerate %d, bps %d", sample_rate, bits_per_sample);
                }
                break;
            }
        default:
            ESP_LOGW(TAG, "%s unhandled event %d", __func__, event);
            break;
    }
}


void bt_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch(event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
        case ESP_A2D_AUDIO_STATE_EVT:
        case ESP_A2D_AUDIO_CFG_EVT:
            work_dispatch(bt_hdl_a2d_evt, event, param, sizeof(esp_a2d_cb_param_t));
            break;
        default:
            ESP_LOGE(TAG, "Unknown A2DP event %d", event);
            break;
    }

}


void bt_a2d_data_cb(const uint8_t *data, uint32_t len) {
    ESP_LOGD(TAG, "Data received: %d bytes", len);

    xRingbufferSend(s_state->buffer.data, data, len, portMAX_DELAY);
}

void av_hdl_avrc_ct_evt(uint16_t event, void *param) {
    ESP_LOGD(TAG, "%s, evt %d", __func__, event);
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *) (param);

    switch(event) {
        case ESP_AVRC_CT_METADATA_RSP_EVT:
        {
            ESP_LOGI(TAG, "AVRC Metadata RSP: attr id 0x%x, %s", rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
            free(rc->meta_rsp.attr_text);
            break;
        }
        case ESP_AVRC_CT_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = rc->conn_stat.remote_bda;
            ESP_LOGI(TAG, "AVRC ct conn_state evt: connected[%d], addr[%02x:%02x:%02x:%02x:%02x:%02x]", rc->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

            if (rc->conn_stat.connected) {
                esp_avrc_ct_send_get_rn_capabilities_cmd(RC_CT_TL_GET_CAPS);
            } else {
                // Clear capability bits
                s_avrc_peer_rn_cap.bits = 0;
            }
            break;
        }
        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
        {
            ESP_LOGI(TAG, "AVRC passthrough rsp: key_code 0x%x, key_state %d", rc->psth_rsp.key_code, rc->psth_rsp.key_state);
            break;
        }
        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
        {
            ESP_LOGI(TAG, "AVRC event notification: %d", rc->change_ntf.event_id);
            /* bt_av_notify_evt_handler(rc->change_ntf.event_id, &rc->change_ntf.event_parameter); */
            break;
        }
        case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
        {
            ESP_LOGI(TAG, "AVRC remote features %x, TG features %x", rc->rmt_feats.feat_mask, rc->rmt_feats.tg_feat_flag);
            break;
        }
        case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
        {
            ESP_LOGI(TAG, "remote rn_cap: count %d, bitmask 0x%x", rc->get_rn_caps_rsp.cap_count,
                     rc->get_rn_caps_rsp.evt_set.bits);
            s_avrc_peer_rn_cap.bits = rc->get_rn_caps_rsp.evt_set.bits;
            /* bt_av_new_track(); */
            /* bt_av_playback_changed(); */
            /* bt_av_play_pos_changed(); */
            break;
        }
        default:
            ESP_LOGE(TAG, "%s unhandled event %d", __func__, event);
            break;
    }
}


void av_hdl_avrc_tg_evt(uint16_t event, void *param) {
    esp_avrc_tg_cb_param_t *rc = (esp_avrc_tg_cb_param_t *) param;
    switch(event) {
        case ESP_AVRC_TG_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = rc->conn_stat.remote_bda;
            ESP_LOGI(TAG, "AVRC tg conn_state evt: connected[%d], addr[%02x:%02x:%02x:%02x:%02x:%02x]", rc->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
            break;
        }
        case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT:
        {
            ESP_LOGI(TAG, "AVRC passthrough cmd: key_code 0x%x, key_state %d", rc->psth_cmd.key_code, rc->psth_cmd.key_state);
            break;
        }
        case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
        {
            ESP_LOGI(TAG, "AVRC set absolute volume: %d%%", (int)rc->set_abs_vol.volume * 100/ 0x7f);
            /* volume_set_by_controller(rc->set_abs_vol.volume); */
            break;
        }
        case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
        {
            ESP_LOGI(TAG, "AVRC register event notification: %d, param: 0x%x", rc->reg_ntf.event_id, rc->reg_ntf.event_parameter);
            /*
            if (rc->reg_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE) {
                s_volume_notify = true;
                esp_avrc_rn_param_t rn_param;
                rn_param.volume = s_volume;
                esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &rn_param);
            }
            */
            break;
        }
        case ESP_AVRC_TG_REMOTE_FEATURES_EVT:
        {
            ESP_LOGI(TAG, "AVRC remote features %x, CT features %x", rc->rmt_feats.feat_mask, rc->rmt_feats.ct_feat_flag);
            break;
        }
        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
    }
}


void bt_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
    switch (event) {
    case ESP_AVRC_CT_METADATA_RSP_EVT:
        {
            esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(param);
            uint8_t *attr_text = (uint8_t *) malloc (rc->meta_rsp.attr_length + 1);
            memcpy(attr_text, rc->meta_rsp.attr_text, rc->meta_rsp.attr_length);
            attr_text[rc->meta_rsp.attr_length] = 0;

            rc->meta_rsp.attr_text = attr_text;
        }
        /* fall through */
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT: {
        work_dispatch(av_hdl_avrc_ct_evt, event, param, sizeof(esp_avrc_ct_cb_param_t));
        break;
    }
    default:
        ESP_LOGE(TAG, "Invalid AVRC event: %d", event);
        break;
    }
}


void bt_rc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param) {
    switch (event) {
        case ESP_AVRC_TG_CONNECTION_STATE_EVT:
        case ESP_AVRC_TG_REMOTE_FEATURES_EVT:
        case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT:
        case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
        case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
            work_dispatch(av_hdl_avrc_tg_evt, event, param, sizeof(esp_avrc_tg_cb_param_t));
            break;
        default:
            ESP_LOGE(TAG, "Invalid AVRC event: %d", event);
            break;
    }
}


void bt_hdl_stack_evt(uint16_t event, void *param) {
    switch(event) {
        case BT_EVT_STACK_UP:
        {
            ESP_LOGD(TAG, "%s, stack_up evt", __func__);
            char *dev_name = "ShockSpeaker";
            esp_bt_dev_set_device_name(dev_name);

            esp_bt_gap_register_callback(bt_gap_cb);

            /* AVRCP Controller */
            ESP_ERROR_CHECK(esp_avrc_ct_init());
            esp_avrc_ct_register_callback(bt_rc_ct_cb);
            /* initialize AVRCP target */
            ESP_ERROR_CHECK(esp_avrc_tg_init());
            esp_avrc_tg_register_callback(bt_rc_tg_cb);

            esp_avrc_rn_evt_cap_mask_t evt_set = {0};
            esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
            ESP_ERROR_CHECK(esp_avrc_tg_set_rn_evt_cap(&evt_set));


            /* A2DP Sink */
            esp_a2d_register_callback(bt_a2d_cb);
            esp_a2d_sink_register_data_callback(bt_a2d_data_cb);
            esp_a2d_sink_init();

            /* Set to connectable */
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

            break;
        }
        default:
            ESP_LOGE(TAG, "%s unhandled event %d", __func__, event);
            break;
    }
}


static void bt_task(void *arg) {
    ESP_LOGD(TAG, "Starting loop");

    msg_t msg;
    for (;;) {

        // Wait if not running
        // TODO: This can probaly be done with an interrupt
        /*
        if (s_state->status != PLAYING) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        */

        /* if (xQueueReceive(s_bt_task_queue, &msg, portMAX_DELAY) == pdTRUE) { */
        if (xQueueReceive(s_bt_task_queue, &msg, 100/portTICK_RATE_MS) == pdTRUE) {
            ESP_LOGD(TAG, "%s, sig 0x%x, evt 0x%x", __func__, msg.signal, msg.event);
            switch(msg.signal) {
                case BT_SIG_WORK_DISPATCH:
                    if (msg.cb)
                        msg.cb(msg.event, msg.param);
                    break;
                default:
                    ESP_LOGW(TAG, "%s, sig 0x%x unknown", __func__, msg.signal);
                    break;
            }

            if (msg.param)
                free(msg.param);
        }
    }
}


bool source_bt_play() {
    ESP_LOGI(TAG, "Playing");

    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not ready");
        return false;
    }

    s_state->status = PLAYING;
    return true;
}


bool source_bt_pause() {
    ESP_LOGI(TAG, "Pausing");

    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not ready");
        return false;
    }

    s_state->status = PAUSED;
    return true;
}


source_state_t *source_bt_init() {
    ESP_LOGI(TAG, "Initializing");

    bt_init();

    s_state = create_source_state(SOURCE_BLUETOOTH, BT_BUF_SIZE);
    if (!s_state->buffer.data) {
        ESP_LOGE(TAG, "Could not allocate memory for source state");
        exit(1);
    }

    s_state->play = &source_bt_play;
    s_state->pause = &source_bt_pause;

    s_bt_task_queue = xQueueCreate(10, sizeof(msg_t));
    xTaskCreate(bt_task, "Bluetooth", 2048, NULL, configMAX_PRIORITIES - 4, s_bt_task_handle);

    work_dispatch(bt_hdl_stack_evt, BT_EVT_STACK_UP, NULL, 0);

    s_state->status = INITIALIZED;
    ESP_LOGI(TAG, "Initialized");

    return s_state;
}


// TODO: Shutdown function
