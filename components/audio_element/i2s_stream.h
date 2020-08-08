#ifndef I2S_STREAM_H
#define I2S_STREAM_H

#include "audio_element.h"


/**
 * Initialize i2s stream
 *
 * @param config    Pointer to a i2s_stream_cfg struct
 *
 * @return
 *      - audio_element_t if successful
 *      - NULL otherwise
 */
audio_element_t *i2s_stream_init(audio_element_cfg_t cfg, audio_stream_type_t type);

#endif
