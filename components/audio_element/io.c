
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

#include "io.h"


#define IO_TICKS_TO_WAIT pdMS_TO_TICKS(1000)

static const char TAG[] = "IO";

size_t _read_rb(io_t *io, char *buf, size_t len, void *pv) {
    size_t bytes_read;
    void *data = xRingbufferReceiveUpTo(io->rb, &bytes_read, 10, len);
    if (data) {
        memcpy(buf, data, bytes_read);
        vRingbufferReturnItem(io->rb, data);

        ESP_LOGV(TAG, "Read %d bytes", bytes_read);
        return bytes_read;
    }
    return 0;
}

size_t _write_rb(io_t *io, char *buf, size_t len, void *pv) {
    size_t bytes_written;
    BaseType_t ret = xRingbufferSend(io->rb, buf, len, IO_TICKS_TO_WAIT);
    if (ret == pdTRUE)
        bytes_written = len;
    else
        bytes_written = IO_WRITE_ERROR;

    ESP_LOGV(TAG, "Written %d bytes", bytes_written);
    return bytes_written;
}

io_t *io_create(io_cb read, io_cb write, int size) {
    io_t *io = calloc(1, sizeof(io_t));

    if (size) {
        io->rb = xRingbufferCreate(size, RINGBUF_TYPE_BYTEBUF);
        io->read =_read_rb;
        io->write =_write_rb;
    } else if (read || write) {
        io->read = read;
        io->write = write;
    } else {
        ESP_LOGE(TAG, "No cb functions or buffer size given.");
        return NULL;
    }
    return io;
}

void io_destroy(io_t *io) {
    if (io->rb) {
        vRingbufferDelete(io->rb);
    }
    if (io->user_data)
        free(io->user_data);
    free(io);
    io = NULL;
}
