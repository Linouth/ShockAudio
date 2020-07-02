#ifndef SOURCE_BLUETOOTH_H
#define SOURCE_BLUETOOTH_H

#include <stdint.h>

#define BT_BUF_SIZE 2048*4


typedef void (* cb_t)(uint16_t event, void *param);
typedef struct {
    uint16_t signal;
    uint16_t event;
    uint8_t  state;
    cb_t     cb;
    void     *param;
} msg_t;


enum signal {
    BT_SIG_WORK_DISPATCH = 0,
    BT_SIG_SEND_PASSTHROUGH = 1,
};

enum bt_stack_event{
    BT_EVT_STACK_UP = 0,
};

enum transmission_layer {
    TL_GET_CAPS = 0,
    TL_GET_METADATA = 1,
    TL_TRACK_CHANGE = 2,
    TL_PLAYBACK_CHANGE = 3,
    TL_PLAY_POS_CHANGE = 4,
    TL_PASSTHROUGH = 5,
};


/**
 * Initialize Bluetooth source
 * Assigns play and pause functions to the structure. Also creates task.
 * @return  State structure
 */
void source_bluetooth_init();

#endif
