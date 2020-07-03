#include "esp_err.h"
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

static char* s_mountpoint = NULL;
static int s_slot = -1;
static sdmmc_card_t* s_card;

esp_err_t sdcard_init(char *mountpoint, int max_files) {
    esp_err_t ret;

    if (s_mountpoint) {
        ESP_LOGE(TAG, "SD Card already initialized and mounted to %s",
                s_mountpoint);
        return ESP_FAIL;
    }

    s_mountpoint = mountpoint;

    // Config options for mounting filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = max_files,
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
        ESP_LOGE(TAG, "Failed to initialize bus. Probably already initialized");
    }

    // Configure SD SPI device
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // Mount sdcard
    ret = esp_vfs_fat_sdspi_mount(mountpoint, &host, &slot_config,
            &mount_config, &s_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.",
                esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}


esp_err_t sdcard_destroy() {
    ESP_LOGI(TAG, "Unmounting sdcard");
    if (s_mountpoint) {
        // Unmount sdcard
        esp_vfs_fat_sdcard_unmount(s_mountpoint, s_card);
        s_mountpoint = NULL;
    } else {
        ESP_LOGW(TAG, "sdcard not mounted. Nothing to destroy.");

        return ESP_FAIL;
    }

    if (s_slot >= 0) {
        //deinitialize the bus after all devices are removed
        spi_bus_free(s_slot);
    }
    return ESP_OK;
}
