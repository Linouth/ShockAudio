#ifndef SOURCE_BLUETOOTH_H
#define SOURCE_BLUETOOTH_H

#include "audio_buffer.h"
#include "audio_source.h"

#include <stdint.h>
#include <stddef.h>

#define BT_BUF_SIZE 2048*4

/**
 * Initialize Bluetooth source
 * Assigns play and pause functions to the structure. Also creates task.
 * @return  State structure
 */
source_state_t *source_bt_init();

#endif
