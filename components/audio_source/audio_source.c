#include "audio_source.h"
#include "audio_buffer.h"
#include <stdio.h>


source_state_t *create_source_state(audio_source_t source, size_t buflen) {
    source_state_t *s = calloc(1, sizeof(source_state_t));

    if (!s)
        return NULL;

    s->source = source;
    s->status = UNINITIALIZED;
    s->buffer.data = xRingbufferCreate(buflen, RINGBUF_TYPE_BYTEBUF);
    return s;
}

void clear_source_state(source_state_t *s) {
    vRingbufferDelete(s->buffer.data);
    free(s);
}
