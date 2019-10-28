#ifndef AUDIO_SOURCE_C
#define AUDIO_SOURCE_C

#include "audio_buffer.h"
#include "config.h"


typedef enum {
    UNINITIALIZED,
    INITIALIZED,
    STOPPED,
    PAUSED,
    RUNNING
} source_status_t ;


typedef struct {
    char *name;
    buffer_t buffer;
    source_status_t status;
} source_state_t;


enum audio_source {
/* #ifdef ENABLE_SDCARD */
    SOURCE_SDCARD = 0,
/* #endif */
#ifdef ENABLE_BLUETOOTH
    SOURCE_BLUETOOTH,
#endif
#ifdef ENABLE_TONE
    SOURCE_TONE,
#endif
    SOURCE_COUNT
};

source_state_t *create_source_state(char *name, size_t buflen);
void clear_source_state(source_state_t *s);

#endif
