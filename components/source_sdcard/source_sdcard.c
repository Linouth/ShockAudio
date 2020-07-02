#include "source.h"
#include "source_sdcard.h"
/* #include "audio_buffer.h" */

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

static TaskHandle_t s_sd_task_handle = NULL;
static source_ctx_t *ctx = NULL;

FILE* fd;
int fd_OK = 0;
bool new_file = false;

static int sd_init() {
    esp_err_t ret;
    sdmmc_card_t* card;

    // Reset 'file_read' flag
    fd_OK = 0;

    // Config options for mounting filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Configure SPI bus
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8128,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        /* return -1; */
    }

    // Configure SD SPI device
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // Mount sdcard
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config,
            &mount_config, &card);

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

static void read_wav_header(uint8_t *data) {
    int channels = (uint16_t)(data[23] << 8 | data[22]);
    uint32_t sample_rate = (uint32_t)(data[27] << 24 | data[26] << 16 | data[25] << 8 | data[24]);
    int bits_per_sample = (uint16_t)(data[35] << 8 | data[34]);

    ESP_LOGI(TAG, "New file: sample_rate %d, channels %d, bitsPerSample %d", sample_rate, channels, bits_per_sample);

    ctx->buffer.format.bits_per_sample = bits_per_sample;
    ctx->buffer.format.channels = channels;
    ctx->buffer.format.sample_rate = sample_rate;
}

// TODO: Check filetype
static void sd_task(void *arg) {
    uint8_t *data = calloc(SDREAD_BUF_SIZE, sizeof(char));
    size_t bytes_read;

    // TODO: This is temp
    esp_log_level_set("spi_master", ESP_LOG_INFO);
    esp_log_level_set("Renderer", ESP_LOG_INFO);
    esp_log_level_set("I2S", ESP_LOG_INFO);

    ESP_LOGD(TAG, "Starting SD loop");
    while (ctx->status != STOPPED) {

        if (ctx->status == PAUSED) {
            ESP_LOGD(TAG, "Paused but running? Pausing task");
            source_pause(SOURCE_SDCARD);
            continue;
        }

        if (!fd_OK) {
            ESP_LOGD(TAG, "No file selected");
            ctx->status = PAUSED;
            continue;
        }

        if (ctx->status != PLAYING) {
            ctx->status = PLAYING;
            fread(data, sizeof(char), 44, fd);
            // TODO: Support more codec, and make this more robust
            // (Crashes if not a wav file)
            read_wav_header(data);
        }
        bytes_read = fread(data, sizeof(char), SDREAD_BUF_SIZE, fd);
        if (bytes_read < SDREAD_BUF_SIZE) {
            ESP_LOGI(TAG, "At end of file");
            fclose(fd);
            fd_OK = 0;
            ctx->status = PAUSED;
        }

        /* ESP_LOGD(TAG, "Wrinting %u bytes to ringbuffer", bytes_read); */
        /* bytes_written = audio_write_ringbuf(data, bytes_read, SOURCE_SDCARD); */
        source_write(SOURCE_SDCARD, data, bytes_read);
    }

    free(data);
    sd_close();
    source_destroy_ctx(ctx);
}

int source_sdcard_play_file(char* filename) {
    if (ctx->status == UNINITIALIZED) {
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
    /* new_file = true; */
    source_play(SOURCE_SDCARD);
    return 0;
}

void source_sdcard_init() {
    ESP_LOGI(TAG, "Initializing");

    while(sd_init() != 0)
        vTaskDelay(1000/portTICK_PERIOD_MS);


    ctx = source_create_ctx("SDCARD", SOURCE_SDCARD, SDREAD_BUF_SIZE,
            &s_sd_task_handle);

    xTaskCreate(sd_task, "SDCard", 2048, NULL, configMAX_PRIORITIES - 4,
            &s_sd_task_handle);

    ctx->status = PAUSED;
    ESP_LOGI(TAG, "Initialized");
}
