#ifndef AUDIO_SOURCE_C
#define AUDIO_SOURCE_C

#include "config.h"

enum audio_source {
#ifdef ENABLE_SDCARD
    SOURCE_SDCARD = 0,
#endif
#ifdef ENABLE_BLUETOOTH
    SOURCE_BLUETOOTH,
#endif
#ifdef ENABLE_TONE
    SOURCE_TONE,
#endif
    SOURCE_COUNT
};


typedef enum {
    UNINITIALIZED,
    INITIALIZED,
    STOPPED,
    PAUSED,
    RUNNING
} source_status_t ;

#endif
