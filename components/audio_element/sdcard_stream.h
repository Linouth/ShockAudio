#ifndef SDCARD_STREAM_H
#define SDCARD_STREAM_H

#include "audio_element.h"

/**
 * sdcard_stream configuration struct
 */
typedef struct sdcard_stream_cfg {
    audio_stream_type_t type;
    int                 buf_size;
    int                 out_rb_size;
    int                 task_stack;
    char*               tag;
} sdcard_stream_cfg_t;

#define DEFAULT_SDCARD_STREAM_CFG() { \
    .type = AEL_STREAM_READER,          \
    .buf_size = 2048,                   \
    .out_rb_size = 2048,                \
    .task_stack = 2048,                 \
}


/**
 * Initialize sdcard stream
 *
 * @param config    Pointer to a sdcard_stream_cfg struct
 *
 * @return 
 *      - audio_element_t if succesful
 *      - NULL otherwise
 */
audio_element_t *sdcard_stream_init(sdcard_stream_cfg_t *config);

#endif
