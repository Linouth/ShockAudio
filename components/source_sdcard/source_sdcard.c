#include "source_sdcard.h"
#include "audio_source.h"
#include "audio_buffer.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

static const char* TAG = "SDCard";

#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13

static xTaskHandle s_sd_task_handle = NULL;
static source_state_t *s_state;

FILE* fd;
int fd_OK = 0;

static int sd_init() {
    fd_OK = 0;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck  = PIN_NUM_CLK;
    slot_config.gpio_cs   = PIN_NUM_CS;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return -1;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    return 0;
}

static void sd_close() {
    if (fd)
        fclose(fd);

    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(TAG, "SDCard Unmounted");
}

// TODO: Check filetype
// TODO: Read in PCM data from wav file
static void sd_task(void *arg) {
    uint8_t *data = calloc(SDREAD_BUF_SIZE, sizeof(char));
    size_t bytes_read;

    ESP_LOGD(TAG, "Starting SD loop");
    for (;;) {

        // Wait if not running
        // TODO: This can probaly be cone with an interrupt
        if (s_state->status != PLAYING) {
            vTaskDelay(100/portTICK_PERIOD_MS);
            continue;
        }

        if (!fd_OK) {
            ESP_LOGD(TAG, "No file selected");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        bytes_read = fread(data, sizeof(char), SDREAD_BUF_SIZE, fd);
        if (bytes_read < SDREAD_BUF_SIZE) {
            ESP_LOGD(TAG, "At end of file");
            fclose(fd);
            fd_OK = 0;
            s_state->status = STOPPED;
        }

        /* ESP_LOGD(TAG, "Wrinting %u bytes to ringbuffer", bytes_read); */
        /* bytes_written = audio_write_ringbuf(data, bytes_read, SOURCE_SDCARD); */
        xRingbufferSend(s_state->buffer.data, data, bytes_read, portMAX_DELAY);
        ESP_LOGD(TAG, "Bytes written to out_buffer: %u", bytes_read);
    }

    // TODO: This will never run
    free(data);
    sd_close();
}

int source_sdcard_play_file(char* filename) {
    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Can't play file, still uninitialized");
        return -1;
    }

    if (fd_OK) {
        // Properly close fd if available
        fclose(fd);
        fd_OK = 0;
    }

    ESP_LOGD(TAG, "Opening file %s", filename);
    fd = fopen(filename, "rb");
    if (!fd)
        return -1;
    fd_OK = 1;
    return 0;
}

bool source_sdcard_play() {
    ESP_LOGI(TAG, "Playing");

    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not ready");
        return false;
    }

    s_state->status = PLAYING;
    return true;
}

bool source_sdcard_pause() {
    ESP_LOGI(TAG, "Pausing");

    if (s_state->status == UNINITIALIZED) {
        ESP_LOGE(TAG, "Source not ready");
        return false;
    }

    s_state->status = PAUSED;
    return true;
}

source_state_t *source_sdcard_init() {
    ESP_LOGI(TAG, "Initializing");

    while(sd_init() != 0)
        vTaskDelay(1000/portTICK_PERIOD_MS);

    s_state = create_source_state(SOURCE_SDCARD, SDREAD_BUF_SIZE);
    if (!s_state->buffer.data) {
        ESP_LOGE(TAG, "Could not allocate memory for source state");
        exit(1);
    }

    s_state->play = &source_sdcard_play;
    s_state->pause = &source_sdcard_pause;

    xTaskCreate(sd_task, "SDCard", 2048, NULL, configMAX_PRIORITIES - 4, s_sd_task_handle);

    s_state->status = INITIALIZED;
    ESP_LOGI(TAG, "Initialized");

    return s_state;
}
