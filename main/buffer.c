#include "buffer.h"

#include "stdlib.h"


buffer_t *create_buffer(size_t len) {
    buffer_t *buf = calloc(1, sizeof(buffer_t));

    if (!buf)
        return NULL;

    buf->data = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    return buf;
}

void clear_buffer(buffer_t *buf) {
    vRingbufferDelete(buf->data);
    free(buf);
}
