#ifndef SOURCE_H
#define SOURCE_H

#include "audio_buffer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_SOURCES_NUM 10

typedef enum {
    SOURCE_SDCARD = 0,
    SOURCE_BLUETOOTH,
    SOURCE_TONE,
    SOURCE_COUNT
} source_t;

typedef enum {
    UNINITIALIZED,
    WAITING,
    STOPPED,
    PAUSED,
    PLAYING
} status_t;

typedef struct {
    source_t source;
    status_t status;
    buffer_t buffer;
    const char *name;
    TaskHandle_t *handle;
} source_ctx_t;


source_ctx_t *source_create_ctx(const char *source_name, source_t source,
                                size_t buflen, TaskHandle_t *handle);
source_ctx_t **source_return_ctxs();
void source_destroy_ctx(source_ctx_t *ctx);

bool source_play(source_t source);
bool source_pause(source_t source);
bool source_stop(source_t source);

void source_write(source_t source, const uint8_t *data, uint32_t len);
void source_write_wait(source_t source, const uint8_t *data, uint32_t len,
                       TickType_t ticksToWait);

#endif
