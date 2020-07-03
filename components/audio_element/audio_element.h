#ifndef AUDIO_ELEMENT_H
#define AUDIO_ELEMENT_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define MAX_RING_BUFFERS 8;

typedef struct audio_element audio_element_t;

typedef enum {
    AEL_STREAM_NONE,
    AEL_STREAM_READER,
    AEL_STREAM_WRITER
} audio_stream_type_t;

typedef enum {
    AEL_MSG_STOP,
    AEL_MSG_COUNT
} msg_id_t;

typedef struct {
    msg_id_t    id;
    char*       data;
    size_t      data_len;
} msg_t;

typedef esp_err_t (*el_msg_cb)(audio_element_t*, msg_t*);

typedef esp_err_t (*el_io_cb)(audio_element_t*);
/* typedef esp_err_t (*el_process_cb)(audio_element_t*); */
typedef size_t (*el_stream_cb)(audio_element_t*, char*, size_t);

enum io_type {
    IO_TYPE_NULL,
    IO_TYPE_CB,
    IO_TYPE_RB,
    IO_TYPE_RB_MULTI,
};

/**
 * Audio Element configuration
 */
typedef struct audio_element_cfg {
    el_io_cb        open;
    el_io_cb        close;
    el_io_cb        destroy;
    el_io_cb        process;
    el_stream_cb    read;           // optional
    el_stream_cb    write;          // optional
    el_msg_cb       msg_handler;    // optional

    int             out_rb_size;    // optional
    int             buf_size;

    int             task_stack;

    char*           tag;
} audio_element_cfg_t;

#define DEFAULT_AUDIO_ELEMENT_CFG() {   \
    .read = NULL,                       \
    .write = NULL,                      \
    .msg_handler = NULL,                \
    .buf_size = 2048,                   \
    .task_stack = 2048,                 \
    .tag = "Unnamed"                    \
}
#define DEFAULT_OUT_RB_SIZE 2048

/**
 * Information about the data stored and returned by the audio element
 */
typedef struct audio_element_info {
    int     sample_rate;
    int     channels;
    int     bit_depth;
    int     bps;
    size_t  byte_pos;
    size_t  bytes;
} audio_element_info_t;

#define DEFAULT_AUDIO_ELEMENT_INFO() {  \
    .sample_rate = 44100,               \
    .channels = 2,                      \
    .bit_depth = 16,                    \
}

/**
 * Holds IO ringbuffer(s) or callback function for audio element
 */
struct io {
    enum io_type type;
    union {
        RingbufHandle_t rb;
        RingbufHandle_t rbs[MAX_RING_BUFFERS];
        el_stream_cb func;
    } data;
    bool ready;
};

/**
 * Audio element, used for anything having to do with audio data
 */
struct audio_element {
    // Functions
    el_io_cb        open;
    el_io_cb        close;
    el_io_cb        destroy;
    el_io_cb        process;
    el_msg_cb       msg_handler;
    /* esp_err_t (*seek)(); */

    // Input/Output ringbuffer/callback function
    struct io       input;
    struct io       output;

    // Task information
    char*           tag;
    TaskHandle_t    task_handle;
    bool            task_running;
    QueueHandle_t   msg_queue;

    bool            is_open;

    // Data stored
    audio_element_info_t info;
    char*           buf;
    void*           data;
};


/**
 * Initialize audio element with provided config.
 *
 * @param config    The configuration (audio_element_cfg_t)
 *
 * @return
 *     - audio_element_t pointer
 *     - NULL
 */
audio_element_t *audio_element_init(audio_element_cfg_t *config);


/**
 * Get the input data of this audio element
 *
 * @param el    Pointer to audio element
 * @param buf   Buffer to write the input data to
 * @param len   Number of bytes to receive
 *
 * @return
 *      - Number of bytes received
 *      - < 0 for an error
 */
size_t audio_element_input(audio_element_t *el, char *buf, size_t len);


/**
 * Write data to the output of this audio element
 *
 * @param el    Pointer to audio element
 * @param buf   Buffer to write to the output
 * @param len   Number of bytes to write
 *
 * @return
 *      - Number of bytes written
 *      - < 0 for an error
 */
size_t audio_element_output(audio_element_t *el, char *buf, size_t len);


/**
 * Send msg to a running audio elements task
 *
 * @param el    Pointer to audio element
 * @param msg   Pointer to filled out message struct
 *
 * @return
 *      - ESP_OK if successful
 *      - ESP_FAIL otherwise
 */
esp_err_t audio_element_msg_send(audio_element_t *el, msg_t *msg);

#endif
