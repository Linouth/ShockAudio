#ifndef I2S_STREAM_H
#define I2S_STREAM_H

#include "audio_element.h"

typedef struct {
    audio_stream_type_t type;
    int                 buf_size;
    int                 task_stack;
    char                *tag;
} i2s_stream_cfg_t;

#define DEFAULT_I2S_STREAM_CFG() { \
    .type = AEL_STREAM_WRITER, \
    .buf_size = 2048, \
    .task_stack = 2048, \
}

/**
 * Initialize i2s stream
 *
 * @param config    Pointer to a i2s_stream_cfg struct
 *
 * @return
 *      - audio_element_t if successful
 *      - NULL otherwise
 */
audio_element_t *i2s_stream_init(i2s_stream_cfg_t *config);

#endif
