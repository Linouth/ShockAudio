#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "mixer.h"
#include "audio_element.h"
#include "io.h"

#include "esp_log.h"

static const char TAG[] = "MIXER";

typedef struct {
    io_t     *inputs[MIXER_MAX_INPUTS];
    uint16_t volumes[MIXER_MAX_INPUTS];
    size_t   count;
} mixer_t;


// Create signed 32-bit integer from a 'byte_count' length sample
int32_t buffer_to_sample(int offset, char *arr, size_t byte_count,
        bool big_endian) {
    int32_t out = 0;
    int i;

    arr += offset;
    if (big_endian) {
        for (i = 0; i < byte_count; i++) {
            out = (out << 8) | arr[i];
        }
    } else {
        for (i = byte_count-1; i >= 0; i--) {
            out = (out << 8) | arr[i];
        }
    }

    uint32_t mask = (1<<8*byte_count) - 1;
    if (out & (1 << (8*byte_count-1))) {
        out = (~out + 1) & mask;
        out = -out;
    }

    return out;
}

// Write 32-bit integer to data array, handling 'byte_count' bytes per sample
void sample_to_buffer(int32_t sample, int offset, char *arr,
        size_t byte_count, bool big_endian) {
    int i;
    arr += offset;

    uint32_t mask = (1<<8*byte_count) - 1;
    if (sample < 0) {
        sample = -sample;
        sample = (~sample + 1) & mask;
    }
    
    if (big_endian) {
        for (i = byte_count-1; i >= 0; i--) {
            arr[i] = (sample & 0xff);
            sample >>= 8;
        }
    } else {
        for (i = 0; i < byte_count; i++) {
            arr[i] = (sample & 0xff);
            sample >>= 8;
        }
    }
}


// TODO: Support big endian? 
static size_t _mixer_process(audio_element_t *el) {
    mixer_t *mixer = el->data;
    audio_element_info_t *info;
    io_t *input;
    unsigned int i_input, j, i_sample;

    uint16_t max_sample_rate = 0,
             max_bits = 0;
    size_t bytes_per_sample = 0,
           max_bytes_per_sample = 0,
           bytes_read,
           max_bytes_read = 0;

    int32_t output[MIXER_BUF_LEN] = {0};
    
    // Loop over every input, and write its samples in int32_t form to a
    // single output array, this way you can easily add the samples from
    // multiple inputs.
    for (i_input = 0; i_input < mixer->count; i_input++) {
        if (!mixer->inputs[i_input])
            break;
        input = mixer->inputs[i_input];
        info = input->user_data;

        // Determine number of bytes per sample
        bytes_per_sample = info->bits > 16 ? 4 : info->bits/8;

        // Find max samplerate
        max_sample_rate = info->sample_rate > max_sample_rate ?
            info->sample_rate : max_sample_rate;
        // And bitwdith, this is used to reconstruct an output buffer
        if (info->bits > max_bits) {
            max_bits = info->bits;
            max_bytes_per_sample = bytes_per_sample;
        }

        // Read this input's audio data
        bytes_read = input->read(input, el->buf, el->buf_len, el);
        max_bytes_read = bytes_read > max_bytes_read ? bytes_read : 
            max_bytes_read;
        
        // TODO: This will mix channels if not all inputs have the same number
        // of channels.
        // Loop over each sample, i_sample holds index of sample in bytes,
        // j is the actual sample number
        for (i_sample = 0, j = 0; i_sample < bytes_read;
                i_sample += bytes_per_sample, j++) {

            // Convert sample in multiple bytes into a single int32_t integer
            // Add it to the existing buffer to mix signals
            output[j] += buffer_to_sample(i_sample, el->buf, bytes_per_sample,
                    false);
            // Crude volume control, output is 'volumes' times divided by two
            output[j] >>= mixer->volumes[i_input];
        }
    }

    // Loop over each sample after they are mixed.
    for (i_sample = 0, j = 0; i_sample < max_bytes_read;
            i_sample += bytes_per_sample, j++) {

        // Convert int32_t sample back into bytes and write them to the
        // internal buffer
        sample_to_buffer(output[j], i_sample, el->buf, max_bytes_per_sample,
                false);
    }

    if (max_bytes_read)
        // Write mixed audio to the output rb
        el->output->write(el->output, el->buf, max_bytes_read, el);
    else {
        ESP_LOGD(TAG, "No bytes written");
        vTaskDelay(100);
    }

    return max_bytes_read;
}


static esp_err_t _mixer_open(audio_element_t *el, void* pv) {

    ESP_LOGI(TAG, "Initialization done");
    el->is_open = true;
    return ESP_OK;
}


static esp_err_t _mixer_close(audio_element_t *el) {
    return ESP_OK;
}


static esp_err_t _mixer_destroy(audio_element_t *el) {
    mixer_t *mixer = el->data;


    ESP_LOGI(TAG, "Destroying Mixer");
    free(mixer);

    return ESP_OK;
}


audio_element_t *mixer_init(io_t *inputs[], size_t count) {
    if (count > MIXER_MAX_INPUTS) {
        ESP_LOGE(TAG, "Too many inputs: %d, a max of %d allowed", count,
                MIXER_MAX_INPUTS);
        return NULL;
    }

    mixer_t *mixer = calloc(1, sizeof(mixer_t));
    if (!mixer) {
        ESP_LOGE(TAG, "Could not allocate memory!");
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        mixer->inputs[i] = inputs[i];
    }
    mixer->count = count;
    mixer->volumes[0] = 0;
    /* mixer->volumes[1] = 4; */

    audio_element_cfg_t cfg = { 0 };

    cfg.open = _mixer_open;
    cfg.close = _mixer_close;
    cfg.destroy = _mixer_destroy;
    cfg.process = _mixer_process;

    cfg.buf_len = 2048;
    cfg.task_stack = 2048*4;
    cfg.out_rb_size = 2048;

    cfg.tag = "mixer";

    // Regular input is unused, as we now handle a list of inputs in process
    cfg.input = IO_UNUSED;

    // Output rb is implicitly created, with read and write functions set to
    // the default rb handling functions

    audio_element_t *el = audio_element_init(&cfg);
    if (!el) {
        ESP_LOGE(TAG, "[%s] could not init audio element.", el->tag);
        return NULL;
    }
    el->data = mixer;

    return el;
}
