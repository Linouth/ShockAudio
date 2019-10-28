#include "audio_source.h"
#include <stdio.h>

// Not sure about this, seems kinda hacky but still pretty cool
#define SOURCE(NAME)  { #NAME, &source_##NAME##_get_status }

source_t sources[SOURCE_COUNT] = {
#ifdef ENABLE_SDCARD
    SOURCE(sdcard)
#endif
#ifdef ENABLE_BLUETOOTH
    SOURCE(bluetooth)
#endif
#if ENABLE_TONE
    SOURCE(tone)
#endif
};


int sources_running() {
    printf("Before loop\n");
    printf("%d\n", SOURCE_COUNT);
    printf("%d\n", SOURCE_SDCARD);
    for (int i = 0; i < SOURCE_COUNT; i++) {
        printf("Inside loop\n");
        printf("Test: %s\n", sources[i].name);
        if ((*sources[i].get_status)() == RUNNING)
            return 1;
    }
    printf("Returning 0\n");
    return 0;
}
