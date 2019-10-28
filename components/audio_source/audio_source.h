#ifndef AUDIO_SOURCE_C
#define AUDIO_SOURCE_C

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
    source_status_t (*get_status) (void);
} source_t;



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


int sources_running();
source_t sources[SOURCE_COUNT];


#endif
