#include "source.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *status_names[] = {
    "UNINITIALIZED", "WAITING", "STOPPED", "PAUSED", "PLAYING"
};

static source_ctx_t *source_ctxs[MAX_SOURCES_NUM + 1] = { NULL };
static int source_counter = 0;

static const char *TAG = "Source";

int source_get_index(source_t needle) {
    int i = 0;
    source_ctx_t *ctx;
    for (i = 0, ctx = source_ctxs[i];
            i < MAX_SOURCES_NUM && i < source_counter;
            ctx = source_ctxs[++i]){
        if (ctx->source == needle)
            return i;
    }
    return -1;
}

source_ctx_t *get_ctx(source_t source) {
    int index = source_get_index(source);
    if (index < 0) {
        ESP_LOGE(TAG, "Source %d does not exist", source);
        return NULL;
    }
    return source_ctxs[index];
}

source_ctx_t *source_create_ctx(const char *source_name, source_t source,
                                size_t buflen, TaskHandle_t *handle) {
    if (source_counter >= MAX_SOURCES_NUM) {
        ESP_LOGE(TAG, "Max number of sources reached: %d", source_counter);
        return NULL;
    }

    int source_index = source_get_index(source);
    if (source_index > 0) {
        ESP_LOGE(TAG, "This source already exists: %d", source);
        return NULL;
    }        

    source_ctxs[source_counter] = calloc(1, sizeof(source_ctx_t));
    source_ctx_t *ctx = source_ctxs[source_counter];
    if (!ctx) {
        ESP_LOGE(TAG, "Could not allocate memory for source context %s",
                source_name);
        return NULL;
    }
    ctx->source = source;
    ctx->status = UNINITIALIZED;
    ctx->buffer.data = xRingbufferCreate(buflen, RINGBUF_TYPE_BYTEBUF);
    if (!ctx->buffer.data) {
        ESP_LOGE(TAG, "%s: Could not allocate memory for source buffer", source_name);
        exit(1);
    }

    ctx->name = source_name;
    ctx->handle = handle;

    source_counter++;
    return ctx;
}

source_ctx_t **source_return_ctxs() {
    return source_ctxs;
}

void source_destroy_ctx(source_ctx_t *ctx) {
    vRingbufferDelete(ctx->buffer.data);
    // Is this going to work? Answer: No.
    // TODO: Fix this
    // deleting the task in the task that is being deleted
    vTaskDelete(ctx->handle);
    free(ctx);
}

bool set_status(source_t source, status_t status) {
    source_ctx_t *ctx = get_ctx(source);
    if (!ctx)
        return false;
    if (ctx->status == UNINITIALIZED) {
        ESP_LOGD(ctx->name, "Can't change status, ctx still uninitialized");
        return false;
    }
    ESP_LOGD(ctx->name, "Changing status: %s", status_names[status]);
    ctx->status = status;
    return true;
}

bool source_play(source_t source) {
    if (!set_status(source, WAITING))
        return false;
    source_ctx_t *ctx = get_ctx(source);
    if (!ctx)
        return false;
    vTaskResume(*ctx->handle);
    return true;
}

bool source_pause(source_t source) {
    if (!set_status(source, PAUSED))
        return false;
    source_ctx_t *ctx = get_ctx(source);
    if (!ctx)
        return false;
    vTaskSuspend(*ctx->handle);
    return true;
}

bool source_stop(source_t source) {
    if (!set_status(source, STOPPED))
        return false;
    return true;
}

status_t source_status(source_t source) {
    source_ctx_t *ctx = get_ctx(source);
    if (!ctx) {
        ESP_LOGE(TAG, "Source %d does not exist", source);
        return -1;
    }
    return ctx->status;
}

void source_write_wait(source_t source, const uint8_t *data, uint32_t len, TickType_t ticksToWait) {
    source_ctx_t *ctx = get_ctx(source);
    ESP_LOGV(ctx->name, "Writing %d bytes to buffer", len);

    xRingbufferSend(ctx->buffer.data, data, len, ticksToWait);
}
void source_write(source_t source, const uint8_t *data, uint32_t len) {
    source_write_wait(source, data, len, portMAX_DELAY);
}
