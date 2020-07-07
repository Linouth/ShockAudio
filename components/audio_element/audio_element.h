#ifndef AUDIO_ELEMENT_H
#define AUDIO_ELEMENT_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define MAX_RING_BUFFERS 8

typedef struct audio_element audio_element_t;

typedef enum {
    AEL_STREAM_NONE,    // Task does no fancy things (e.g. throughput / buffer)
    AEL_STREAM_READER,  // Task reads data from source, and writes to rb
    AEL_STREAM_WRITER   // Task writes data to source, and reads from rb
} audio_stream_type_t;

typedef enum {
    AEL_STATUS_PLAYING,  // When audio data is being processed
    AEL_STATUS_PAUSED,   // Manually paused, wait for manual playing
    AEL_STATUS_WAITING,  // Task is waiting for input data
    AEL_STATUS_STOPPED,  // Task is stopped
} audio_element_status_t;

typedef enum {
    AEL_MSG_STATUS_CHANGE,
    AEL_MSG_STOP,
    AEL_MSG_COUNT
} audio_element_msg_id_t;

typedef struct audio_element_msg {
    audio_element_msg_id_t  id;
    char                    *data;
    size_t                  data_len;
} audio_element_msg_t;

typedef esp_err_t (*el_msg_cb)(audio_element_t*, audio_element_msg_t*);

typedef esp_err_t (*el_io_cb)(audio_element_t*);
typedef esp_err_t (*el_open_cb)(audio_element_t*, void*);
typedef size_t (*el_process_cb)(audio_element_t*);
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
    el_open_cb      open;
    el_io_cb        close;
    el_io_cb        destroy;
    el_process_cb   process;
    el_stream_cb    read;           // optional
    el_stream_cb    write;          // optional
    el_msg_cb       msg_handler;    // optional

    int             out_rb_size;    // optional
    int             buf_size;

    int             task_stack;

    char            *tag;
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

    // Pass these through void* in open()
    /* int     duration;   // Used for 'tone' */
    /* char    *uri; */
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
    el_open_cb      open;
    el_io_cb        close;
    el_io_cb        destroy;
    el_process_cb   process;
    el_msg_cb       msg_handler;
    /* esp_err_t (*seek)(); */

    // Input/Output ringbuffer/callback function
    struct io       input;
    struct io       output;

    // Task information
    char            *tag;
    TaskHandle_t    task_handle;
    bool            task_running;
    QueueHandle_t   msg_queue;
    audio_element_status_t status;

    bool            is_open;
    bool            wait_for_notify;

    // Data stored
    audio_element_info_t info;
    char            *buf;
    int             buf_size;
    void            *data;
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
 * Send notification to the task
 *
 * @param el    Pointer to audio element
 * @param bits  The bits to set in the notification
 *
 * @return
 *      - ESP_OK if succesful
 *      - ESP_FAIL otherwise
 */
esp_err_t audio_element_notify(audio_element_t *el, int bits);


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
esp_err_t audio_element_msg_send(audio_element_t *el, audio_element_msg_t *msg);


/**
 * Create audio_element_msg struct to send to task
 *
 * @param id    The audio_element_msg_id to send
 * @param size  Size of the data you want to include
 *
 * @return
 *      - An audio_element_msg struct with the set id, and a data pointer to
 *      'size' allocated bytes
 */
audio_element_msg_t audio_element_msg_create(audio_element_msg_id_t id, size_t size);


/**
 * Set the state of an AEL to playing
 *
 * This is the only way to unblock a 'PAUSED' AEL
 */
void audio_element_play(audio_element_t *el);

/**
 * Set the state of an AEL to paused
 *
 * call `audio_element_play()` to unpause the AEL
 */
void audio_element_pause(audio_element_t *el);

#endif
