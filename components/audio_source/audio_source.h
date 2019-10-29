#ifndef AUDIO_SOURCE_C
#define AUDIO_SOURCE_C

#include "audio_buffer.h"


/**
 * Status for sources
 */
typedef enum {
    UNINITIALIZED,
    INITIALIZED,
    STOPPED,
    PAUSED,
    PLAYING
} source_status_t ;

/**
 * Source identifiers
 */
typedef enum {
    SOURCE_SDCARD = 0,
    SOURCE_BLUETOOTH,
    SOURCE_TONE,
    SOURCE_COUNT
} audio_source_t ;


/**
 * A structure to keep track of a sources state and its buffer
 */
typedef struct source_state {
    audio_source_t source;
    buffer_t buffer;
    source_status_t status;

    bool (*play)();
    bool (*pause)();
} source_state_t;

/**
 * @brief   Allocate and initialize a new source_state struct
 * @param   source: audio source
 *          buflen: size of data ringbuffer
 * @return  NULL: failed to allocate memory
 *          source_state_t: success
 */
source_state_t *create_source_state(audio_source_t source, size_t buflen);

/**
 * @brief   Free allocated memory of source_state struct
 */
void clear_source_state(source_state_t *s);

#endif
