#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "audio_element.h"
#include "i2s_stream.h"
#include "io.h"

#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s.h"

static const char TAG[] = "I2S_STREAM";


typedef struct {
    audio_stream_type_t type;
    int i2s_num;
} i2s_stream_t;


// TODO: Rewrite this whole function
#define BUF_COUNT 4
#define BUF_LEN 2  // Times DMA
#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN 1024
static esp_err_t _i2s_open(audio_element_t *el, void* pv) {
    audio_element_info_t info = el->info;
    i2s_stream_t *stream = el->data;

    ESP_LOGE(TAG, "%d", info.bps);

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = info.sample_rate,
        .bits_per_sample = info.bit_depth,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        /* .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB, */
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,  // Maybe
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .tx_desc_auto_clear = true
    };

    ESP_LOGI(TAG, "Initializing I2S");

    stream->i2s_num = 0;
    i2s_driver_install(stream->i2s_num, &i2s_config, 0, NULL);
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = 27,
        .ws_io_num = 33,
        .data_out_num = 32,
        /* .bck_io_num = 26, */
        /* .ws_io_num = 25, */
        /* .data_out_num = 22, */
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin(stream->i2s_num, &pin_config);

    ESP_LOGI(TAG, "Initialization done");

    el->is_open = true;

    return ESP_OK;
}


static esp_err_t _i2s_close(audio_element_t *el) {
    return ESP_OK;
}


static esp_err_t _i2s_destroy(audio_element_t *el) {
    i2s_stream_t *stream = el->data;

    ESP_LOGI(TAG, "Destroying I2S");
    i2s_driver_uninstall(stream->i2s_num);

    return ESP_OK;
}


static size_t _i2s_write(io_t *io, char *buf, size_t len, void *pv) {
    i2s_stream_t *stream = ((audio_element_t *)pv)->data;

    size_t bytes_written;
    i2s_write(stream->i2s_num, buf, len, &bytes_written, portMAX_DELAY);

    ESP_LOGV(TAG, "Bytes written to i2s: %d", bytes_written);

    return bytes_written;
}


audio_element_t *i2s_stream_init(audio_element_cfg_t cfg, audio_stream_type_t type) {
    i2s_stream_t *stream = calloc(1, sizeof(i2s_stream_t));
    if (!stream) {
        ESP_LOGE(TAG, "Could not allocate memory!");
        return NULL;
    }

    // i2s_init();

    cfg.open = _i2s_open;
    cfg.close = _i2s_close;
    cfg.destroy = _i2s_destroy;

    cfg.tag = "i2s";

    stream->type = type;
    if (type == AEL_STREAM_WRITER) {
        cfg.write = _i2s_write;
    } else {
        ESP_LOGE(TAG, "AEL_STREAM_READER Not implemented yet.");
        return NULL;
    }

    audio_element_t *el = audio_element_init(&cfg);
    if (!el) {
        ESP_LOGE(TAG, "[%s] could not init audio element.", el->tag);
        return NULL;
    }
    el->data = stream;

    return el;
}
