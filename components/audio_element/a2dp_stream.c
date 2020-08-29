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

enum tl {
    TL_GET_CAPS,
    TL_GET_METADATA
};

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
    esp_avrc_rn_evt_cap_mask_t peer_caps;
} a2dp_stream_t;


static QueueHandle_t   gs_msg_queue;
static io_t            *gs_output;


/**
 * BT A2DP functions
 **/

// Helper functions

static inline bool is_capable(a2dp_stream_t *stream, esp_avrc_rn_event_ids_t cap) {
    return esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST, &stream->peer_caps, cap);
}

static void list_caps(a2dp_stream_t *stream) {
    ESP_LOGI(TAG, "Capabilities:");
    if(is_capable(stream, ESP_AVRC_RN_PLAY_STATUS_CHANGE)) {
        // Track status change, eg. from playing to paused
        ESP_LOGI(TAG, "ESP_AVRC_RN_PLAY_STATUS_CHANGE: track status change, eg. playing to paused");
    }
	if(is_capable(stream, ESP_AVRC_RN_TRACK_CHANGE)) {
        // New track is loaded
        ESP_LOGI(TAG, "ESP_AVRC_RN_TRACK_CHANGE: new track is loaded");
    }
	if(is_capable(stream, ESP_AVRC_RN_TRACK_REACHED_END)) {
        // Current track reached end
        ESP_LOGI(TAG, "ESP_AVRC_RN_TRACK_REACHED_END: curr track reached end");
    }
	if(is_capable(stream, ESP_AVRC_RN_TRACK_REACHED_START)) {
        // Curr track reached start
        ESP_LOGI(TAG, "ESP_AVRC_RN_TRACK_REACHED_START: curr track reached start");
    }
	if(is_capable(stream, ESP_AVRC_RN_PLAY_POS_CHANGED)) {
        // track playing position changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_PLAY_POS_CHANGED");
    }
	if(is_capable(stream, ESP_AVRC_RN_BATTERY_STATUS_CHANGE)) {
        // battery status changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_BATTERY_STATUS_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_SYSTEM_STATUS_CHANGE)) {
        // system status changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_SYSTEM_STATUS_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_APP_SETTING_CHANGE)) {
        // Application settings changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_APP_SETTING_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_NOW_PLAYING_CHANGE)) {
        // now playing content changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_NOW_PLAYING_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_AVAILABLE_PLAYERS_CHANGE)) {
        // available players changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_AVAILABLE_PLAYERS_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_ADDRESSED_PLAYER_CHANGE)) {
        // the addressed player changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_ADDRESSED_PLAYER_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_UIDS_CHANGE)) {
        // UIDs changed
        ESP_LOGI(TAG, "ESP_AVRC_RN_UIDS_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_VOLUME_CHANGE)) {
        // volume changed locally on TG
        ESP_LOGI(TAG, "ESP_AVRC_RN_VOLUME_CHANGE");
    }
	if(is_capable(stream, ESP_AVRC_RN_MAX_EVT)) {
        // number of events?
        ESP_LOGI(TAG, "ESP_AVRC_RN_MAX_EVT");
    }
}

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
    ESP_LOGV(TAG, "Dispatch event 0x%x, param len %d", event, param_len);

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
    audio_element_info_t *info = el->output->user_data;
    audio_element_info_t new_info = { 0 };
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
                new_info.sample_rate = sample_rate;

                // Retrieve current number of channels
                uint8_t channels = 2;
                if (oct0 & (1 << 3))
                    channels = 1;
                new_info.channels = channels;

                // Retrieve current bits per second
                uint16_t bits_per_sample = 8;
                uint8_t oct1 = a2d->audio_cfg.mcc.cie.sbc[1];
                if (oct1 & (1 << 5) || oct1 & (1 << 4))
                    bits_per_sample = 16;
                new_info.bits = bits_per_sample;

                audio_element_set_info(info, new_info);
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

// Handle messages sent by the CT
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

        case ESP_AVRC_TG_REMOTE_FEATURES_EVT:
        {
            ESP_LOGI(TAG, "AVRC TG REMOTE_FEATURES_EVT: 0x%x",
                    rc->rmt_feats.feat_mask);
            break;
        }

        case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT:
        {
            ESP_LOGI(TAG, "AVRC TG PASSTHROUGH_CMD_EVT: 0x%x %d",
                    rc->psth_cmd.key_code, rc->psth_cmd.key_state);
            break;
        }

        case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
        {
            ESP_LOGI(TAG, "AVRC TG Set absolute volume cmd received. val: %d",
                    rc->set_abs_vol.volume);
            break;
        }

        case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
        {
            ESP_LOGI(TAG, "AVRC TG REGISTER_NOTIFICATION_EVT: 0x%x,"
                    "param: 0x%x",
                    rc->reg_ntf.event_id, rc->reg_ntf.event_parameter);
            break;
        }

        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
    }
}

// Handle responses to a msg received from the TG
static void bt_hdl_rc_ct_evt(audio_element_t *el, uint16_t event,
        void *param) {
    ESP_LOGD(TAG, "%s avrc controller evt %d", __func__, event);
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *) (param);
    a2dp_stream_t *stream = el->data;

    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT:
        {
            uint8_t *bda = rc->conn_stat.remote_bda;
            ESP_LOGI(TAG, "AVRC ct conn_state evt: %s, addr[%02x:%02x:%02x:%02x:%02x:%02x]",
                    s_a2d_conn_state_str[rc->conn_stat.connected],
                    bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

            // If controller has a connection, ask the remote what it can do
            if (rc->conn_stat.connected) {
                esp_avrc_ct_send_get_rn_capabilities_cmd(TL_GET_CAPS);
            } else {
                stream->peer_caps.bits = 0;
            }
            break;
        }

        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
        {
            ESP_LOGI(TAG, "AVRC CT PASSTHROUGH_RSP_EVT: tl: %d, kc: 0x%x,"
                    "ks: %d", rc->psth_rsp.tl, rc->psth_rsp.key_code,
                    rc->psth_rsp.key_state);
            break;
        }

        case ESP_AVRC_CT_METADATA_RSP_EVT:
        {
            ESP_LOGI(TAG, "AVRC Metadata RSP: attr id 0x%x, %s",
                    rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
            free(rc->meta_rsp.attr_text);
            break;
        }

        case ESP_AVRC_CT_PLAY_STATUS_RSP_EVT:
        {
            ESP_LOGI(TAG, "AVRC CT PLAY_STATUS_RSP_EVT");
            break;
        }

        // NOTE: Every time a registered notification has been handled, you 
        // need to register it again
        // Notification received from TG
        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
        {
            ESP_LOGD(TAG, "AVRC CT CHANGE_NOTIFY_EVT: 0x%x",
                    rc->change_ntf.event_id);

            bt_hdl_notify_evt(rc->change_ntf.event_id,
                    &rc->change_ntf.event_parameter);

            // TODO: implement actual 'capability' notifications
            /* esp_avrc_ct_send_register_notification_cmd(10, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0); */
            esp_avrc_ct_send_register_notification_cmd(10, ESP_AVRC_RN_PLAY_POS_CHANGED, 0);
            break;
        }

        // Once connected, CT asks TG what its caps are. Here is the response
        case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
        {
            ESP_LOGI(TAG, "AVRC CT GET_RN_CAPS_RESP_EVT:"
                    "count %d, bitmask 0x%x",rc->get_rn_caps_rsp.cap_count,
                    rc->get_rn_caps_rsp.evt_set.bits);
            stream->peer_caps.bits = rc->get_rn_caps_rsp.evt_set.bits;
            list_caps(stream);

            /* esp_avrc_ct_send_register_notification_cmd(10, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0); */
            esp_avrc_ct_send_register_notification_cmd(10, ESP_AVRC_RN_PLAY_POS_CHANGED, 0);
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

    gs_output->write(gs_output, (char *)data, len, NULL);
}

static void bt_rc_ct_cb(esp_avrc_ct_cb_event_t event,
        esp_avrc_ct_cb_param_t *param) {
    // NOTE: Possibly handle some events here, so they are handled more quickly? 
    work_dispatch(bt_hdl_rc_ct_evt, event, param,
            sizeof(esp_avrc_ct_cb_param_t));
}

static void bt_rc_tg_cb(esp_avrc_tg_cb_event_t event,
        esp_avrc_tg_cb_param_t *param) {
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

    // Set global msg_queue and ringbuf to this element's
    // This is needed so that the bluetooth callback functions can access this
    // queue and the output buffer
    gs_msg_queue = stream->msg_queue;
    gs_output = el->output;
    
    // Set the device name
    ESP_LOGI(TAG, "[%s] Setting device name to %s", el->tag, device_name);
    esp_bt_dev_set_device_name(device_name);

    // GAP callback 
    esp_bt_gap_register_callback(bt_gap_cb);

    /* NOTE: AVRCP CT > TG explaination:
     * CT is always the device asking TG to do something, esp and phone can
     * both be CT and TG. e.g.:
     * Esp (CT) asks phone (TG) what song is playing. CT cb is called with response
     * or phone (CT) asks esp to set volume to x. TG cb is called with request
     */
    // AVRCP Controller callback
    ESP_ERROR_CHECK(esp_avrc_ct_init());
    esp_avrc_ct_register_callback(bt_rc_ct_cb);
    // AVRCP Target callback
    ESP_ERROR_CHECK(esp_avrc_tg_init());
    esp_avrc_tg_register_callback(bt_rc_tg_cb);

    /* TODO: Is this good for anything?
    esp_avrc_rn_evt_cap_mask_t evt_set = {0};

    esp_avrc_tg_get_rn_evt_cap(ESP_AVRC_RN_CAP_ALLOWED_EVT, &evt_set);
    ESP_LOGI(TAG, "supported cap: 0x%x", evt_set.bits);

    esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
    ESP_ERROR_CHECK(esp_avrc_tg_set_rn_evt_cap(&evt_set));
    */


    // A2DP Sink
    // Handles a2d connection, stream state and codec configuration
    esp_a2d_register_callback(bt_a2d_cb);
    // Handles actual data received
    esp_a2d_sink_register_data_callback(bt_a2d_data_cb);
    esp_a2d_sink_init();

    // Enable discovery, and contectability
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    
    el->is_open = true;
    return ESP_OK;
}

// TODO: Test
static esp_err_t _a2dp_close(audio_element_t *el) {
    gs_msg_queue = NULL;
    gs_output = NULL;

    // Deinit a2dp and avrcp
    ESP_ERROR_CHECK(esp_avrc_ct_deinit());
    ESP_ERROR_CHECK(esp_avrc_tg_deinit());
    ESP_ERROR_CHECK(esp_a2d_sink_deinit());

    // Disable discovery, and contectability
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

    el->is_open = false;
    return ESP_OK;
}

//TODO
static esp_err_t _a2dp_destroy(audio_element_t *el) {
    /* a2dp_stream_t *stream = el->data; */

    // Destory queue
    // free stream
    // Deinit bluetooth

    return ESP_OK;
}

/* This process function does not actually process audio data, instead it
 * checks for bluetooth events, and executes linked functions.
 * The audio data is read into the output buffer through a a2dp callback func.
 */
static size_t _a2dp_process(audio_element_t *el) {
    static msg_t msg;
    /* static a2dp_stream_t *stream = el->data; */

    if (xQueueReceive(gs_msg_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGD(TAG, "[%s] Received a msg to handle. Event: 0x%x", el->tag,
                msg.event);

        if (msg.func)
            msg.func(el, msg.event, msg.param);
        
        if (msg.param)
            free(msg.param);
    }
    return 0;
}

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
    if (type == AEL_STREAM_WRITER) {
        ESP_LOGE(TAG, "AEL_STREAM_WRITER Not implemented.");
        return NULL;
    }

    // Input is not used, as the output RB is being written by a cb
    cfg.input = IO_UNUSED;
    // Output is implicitly created with default rb read and write cb functions

    audio_element_t *el = audio_element_init(&cfg);
    if (!el) {
        ESP_LOGE(TAG, "[%s] could not init audio element.", el->tag);
        return NULL;
    }
    el->data = stream;

    ESP_LOGI(TAG, "[%s] Initializing bluetooth controller", el->tag);
    bt_init();

    ESP_LOGI(TAG, "[%s] Initialization done.", el->tag);

    return el;
}
