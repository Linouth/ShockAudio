#ifndef SDCARD_STREAM_H
#define SDCARD_STREAM_H

#include "audio_element.h"


/**
 * Initialize sdcard stream
 *
 * @param config    Pointer to a sdcard_stream_cfg struct
 *
 * @return 
 *      - audio_element_t if succesful
 *      - NULL otherwise
 */
audio_element_t *sdcard_stream_init(audio_element_cfg_t cfg, audio_stream_type_t type);

#endif
