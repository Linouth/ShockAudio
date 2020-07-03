#ifndef SDCARD_H
#define SDCARD_H

#include "esp_err.h"

/**
 * Initialize the sdcard (over SPI)
 *
 * @param mountpoint    Directory to mount to
 * @param max_files     Max number of files that can be opened at the same time.
 *
 * @return
 *      - ESP_OK if successful
 *      - ESP_FAIL otherwise
 */
esp_err_t sdcard_init(char *mountpoint, int max_files);


/**
 * Destroy and cleanup sdcard
 */
esp_err_t sdcard_destroy();

#endif
