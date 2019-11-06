#include "source_bluetooth.h"
#include "audio_source.h"
#include "audio_buffer.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_a2dp_api.h"
#include "esp_gap_bt_api.h"

static const char* TAG = "Bluetooth";

static const char *s_a2d_conn_state_str[] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
static const char *s_a2d_audio_state_str[] = {"Suspended", "Stopped", "Started"};

static xTaskHandle s_bt_task_handle = NULL;
static source_state_t *s_state;

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

void bt_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
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
                    ESP_LOGW(TAG, "Unimplemented Audio State EVT");
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
            ESP_LOGW(TAG, "Unimplemented A2DP event: %d", event);
            break;
    }
}

void bt_a2d_data_cb(const uint8_t *data, uint32_t len) {
    ESP_LOGD(TAG, "Data received: %d bytes", len);

    xRingbufferSend(s_state->buffer.data, data, len, portMAX_DELAY);
}

// TODO: Also hacky...
void bt_stack_up() {
    char *dev_name = "ShockSpeaker";
    esp_bt_dev_set_device_name(dev_name);
    esp_bt_gap_register_callback(bt_gap_cb);

    esp_a2d_register_callback(bt_a2d_cb);
    esp_a2d_sink_register_data_callback(bt_a2d_data_cb);
    esp_a2d_sink_init();

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // TODO: Add AVRCP controller
}

static void bt_task(void *arg) {
    uint8_t *data = calloc(BT_BUF_SIZE, sizeof(char));
    size_t bytes_read;

    bt_stack_up();

    ESP_LOGD(TAG, "Starting loop");
    for (;;) {

        // Wait if not running
        // TODO: This can probaly be cone with an interrupt
        if (s_state->status != PLAYING) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }


        /* ESP_LOGD(TAG, "Wrinting %u bytes to ringbuffer", bytes_read); */
        /* bytes_written = audio_write_ringbuf(data, bytes_read, SOURCE_SDCARD); */
        /* xRingbufferSend(s_state->buffer.data, data, bytes_read, portMAX_DELAY); */
        /* ESP_LOGD(TAG, "Bytes written to out_buffer: %u", bytes_read); */

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // TODO: This will never run
    free(data);
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

    xTaskCreate(bt_task, "Bluetooth", 2048, NULL, configMAX_PRIORITIES - 4, s_bt_task_handle);

    s_state->status = INITIALIZED;
    ESP_LOGI(TAG, "Initialized");

    return s_state;
}
