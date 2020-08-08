#ifndef AUDIO_ELEMENT_H
#define AUDIO_ELEMENT_H

#include "io.h"

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


typedef esp_err_t (*el_io_cb)(audio_element_t*);
typedef esp_err_t (*el_open_cb)(audio_element_t*, void*);
typedef size_t (*el_process_cb)(audio_element_t*);
typedef size_t (*el_stream_cb)(audio_element_t*, char*, size_t);

/**
 * Audio Element configuration
 */
typedef struct audio_element_cfg {
    el_open_cb      open;
    el_io_cb        close;
    el_io_cb        destroy;
    el_process_cb   process;        // optional
    io_cb           read;           // optional
    io_cb           write;          // optional
    io_t            *input;         // optional
    io_t            *output;        // optional

    int             out_rb_size;    // optional
    int             buf_len;

    int             task_stack;

    char            *tag;
} audio_element_cfg_t;

#define DEFAULT_AUDIO_ELEMENT_CFG() {   \
    .read = NULL,                       \
    .write = NULL,                      \
    .buf_len = 2048,                    \
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
 * Audio element, used for anything having to do with audio data
 */
struct audio_element {
    // Functions
    el_open_cb      open;
    el_io_cb        close;
    el_io_cb        destroy;
    el_process_cb   process;

    // Input/Output ringbuffer/callback function
    io_t            *input;
    io_t            *output;

    // Task information
    char            *tag;
    TaskHandle_t    task_handle;
    bool            task_running;
    QueueHandle_t   msg_queue;
    audio_element_status_t status;

    bool            is_open;

    // Data stored
    audio_element_info_t info;
    char            *buf;
    int             buf_len;
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


void audio_element_change_status(audio_element_t *el);

#endif
