#ifndef SOURCE_H
#define SOURCE_H

#include "audio_buffer.h"

#define MAX_SOURCES_NUM 10


typedef enum {
    SOURCE_SDCARD = 0,
    SOURCE_BLUETOOTH,
    SOURCE_TONE,
    SOURCE_COUNT
} source_t;

typedef enum {
    UNINITIALIZED,
    INITIALIZED,
    STOPPED,
    PAUSED,
    PLAYING
} status_t;

typedef struct {
    source_t source;
    status_t status;
    buffer_t buffer;
    char *name;
} source_ctx_t;


source_ctx_t *create_source_ctx(char *source_name, source_t source, size_t buflen);

#define play(source)        source_change_status(source, PLAYING)
#define pause(source)       source_change_status(source, PAUSED)
#define stop(source)        source_change_status(source, STOPPED)
bool source_change_status(source_t source, status_t status);
status_t source_status(source_t source);

void source_write(source_t source, const uint8_t *data, uint32_t len);
void source_write_wait(source_t source, const uint8_t *data, uint32_t len, TickType_t ticksToWait);

#endif
