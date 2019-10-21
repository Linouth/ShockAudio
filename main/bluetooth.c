#include "bluetooth.h"
#include "audio.h"

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

static xTaskHandle s_bt_task_handle = NULL;

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
    switch (event) {
        case (ESP_A2D_AUDIO_CFG_EVT):
            {
                ESP_LOGD(TAG, "Audio Codec configured:");
                break;
            }
        default:
            ESP_LOGW(TAG, "Unimplemented A2DP event: %d", event);
            break;
    }
}

void bt_a2d_data_cb(const uint8_t *data, uint32_t len) {
    ESP_LOGD(TAG, "Data received: %d bytes", len);

    audio_write_ringbuf(data, len, SOURCE_BLUETOOTH);
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

void bt_task(void *arg) {
    struct audio_state *state = (struct audio_state *) arg;

    bt_stack_up();

    for (;;) {
        if (state->buffer_assigned[SOURCE_BLUETOOTH] == -1) {
            ESP_LOGD(TAG, "No buffer assigned");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void bt_task_start(struct audio_state *state) {
    bt_init();

    xTaskCreate(bt_task, "Bluetooth", 2048, state, configMAX_PRIORITIES - 3, s_bt_task_handle);
}
