#ifndef IO_H
#define IO_H

#include <stddef.h>
#include <stdbool.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

enum IO_ERROR {
    IO_WRITE_ERROR = -2
};

typedef struct io_ io_t;
typedef size_t (*io_cb)(io_t *io, char *buf, size_t len, void *pv);

struct io_ {
    io_cb read;
    io_cb write;
    RingbufHandle_t rb;
};

/**
 * Create a new IO struct for input and/or output handling.
 *
 * Only one if `read` and `write` has to be set, or if size is set, no
 * callback functions are required.
 *
 * @param read Callback function to read data
 * @param write Callback function to write data
 * @param size Size of the buffer
 *
 * @returns io_t Struct
 */
io_t *io_create(io_cb read, io_cb write, int size);

/**
 * Destroy and free io_t struct
 *
 * @param io Pointer to io_t struct to destroy
 */
void io_destroy(io_t *io);

#endif
