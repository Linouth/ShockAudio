#include "source.h"

#include "esp_log.h"

static const char *status_names[] = {
    "UNINITIALIZED", "INITIALIZED", "STOPPED", "PAUSED", "PLAYING"
};

static source_ctx_t *source_ctxs[MAX_SOURCES_NUM];
static int source_ctxs_lut[MAX_SOURCES_NUM];
static int source_counter = 0;

static const char *TAG = "Source";

int source_get_index(int needle) {
    for (int e, i = 0; i < MAX_SOURCES_NUM && i < source_counter; i++) {
        e = source_ctxs_lut[i];
        if (e == needle)
            return i;
    }
    return -1;
}

source_ctx_t *get_source_ctx(source_t source) {
    int index = source_get_index(source);
    if (index < 0)
        return NULL;
    return source_ctxs[index];
}

source_ctx_t *create_source_ctx(char *source_name, source_t source, size_t buflen) {
    if (source_counter >= MAX_SOURCES_NUM) {
        ESP_LOGE(TAG, "Max number of sources reached: %d", source_counter);
        return NULL;
    }

    int source_index = source_get_index(source);
    if (source_index > 0) {
        ESP_LOGE(TAG, "This source already exists: %d", source);
        return NULL;
    }        

    source_ctx_t *ctx = source_ctxs[source_counter];
    ctx = calloc(1, sizeof(source_ctx_t));
    if (!ctx)
        return NULL;
    ctx->source = source;
    ctx->status = UNINITIALIZED;
    ctx->buffer.data = xRingbufferCreate(buflen, RINGBUF_TYPE_BYTEBUF);
    if (!ctx->buffer.data) {
        ESP_LOGE(TAG, "%s: Could not allocate memory for source state", source_name);
        exit(1);
    }

    ctx->name = source_name;

    source_counter++;
    return ctx;
}

bool source_change_status(source_t source, status_t status) {
    source_ctx_t *ctx = get_source_ctx(source);
    if (!ctx) {
        ESP_LOGE(TAG, "Source %d does not exist", source);
        return false;
    }
    if (status != INITIALIZED && ctx->status == UNINITIALIZED) {
        ESP_LOGD(ctx->name, "Can't change status, ctx still uninitialized");
        return false;
    }
    ESP_LOGD(ctx->name, "Changing status: %s", status_names[status]);
    ctx->status = status;
    return true;
}

status_t source_status(source_t source) {
    source_ctx_t *ctx = get_source_ctx(source);
    if (!ctx) {
        ESP_LOGE(TAG, "Source %d does not exist", source);
        return -1;
    }
    return ctx->status;
}

void source_write_wait(source_t source, const uint8_t *data, uint32_t len, TickType_t ticksToWait) {
    source_ctx_t *ctx = get_source_ctx(source);
    ESP_LOGV(ctx->name, "Writing %d bytes to buffer", len);

    xRingbufferSend(ctx->buffer.data, data, len, ticksToWait);
}
void source_write(source_t source, const uint8_t *data, uint32_t len) {
    source_write_wait(source, data, len, portMAX_DELAY);
}
