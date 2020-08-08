
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

#include "io.h"


#define IO_TICKS_TO_WAIT pdMS_TO_TICKS(100)

static const char TAG[] = "IO";

size_t _read_rb(io_t *io, char *buf, size_t len) {
    size_t bytes_read;
    void *data = xRingbufferReceiveUpTo(io->rb, &bytes_read,
            IO_TICKS_TO_WAIT, len);
    memcpy(buf, data, bytes_read);
    vRingbufferReturnItem(io->rb, data);

    ESP_LOGV(TAG, "Read %d bytes", bytes_read);
    return bytes_read;
}

size_t _write_rb(io_t *io, char *buf, size_t len) {
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

    if (read || write) {
        io->read = read;
        io->write = write;
    } else if (size) {
        io->rb = xRingbufferCreate(size, RINGBUF_TYPE_BYTEBUF);
        io->read =_read_rb;
        io->write =_write_rb;
    } else {
        ESP_LOGE(TAG, "No cb functions or buffer size given.");
        return NULL;
    }
    return io;
}
