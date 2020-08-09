#ifndef A2DP_STREAM_H
#define A2DP_STREAM_H

#include "audio_element.h"


/**
 * Initialize bluetooth A2DP stream
 *
 * @param cfg A configured `audio_element_cfg_t` struct
 *
 * @return
 *      - audio_element_t if successful
 *      - NULL otherwise
 */
audio_element_t *a2dp_stream_init(audio_element_cfg_t cfg, audio_stream_type_t type);


#endif
