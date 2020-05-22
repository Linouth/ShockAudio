#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include <string.h>

#include "mixer.h"
#include "source.h"
#include "audio_buffer.h"
#include "audio_renderer.h"


static const char* TAG = "Mixer";

static xTaskHandle s_mixer_task_handle = NULL;

static source_ctx_t **ctxs;


// Create signed 32-bit integer from 'bytes' length sample
int32_t buffer_to_sample(int offset, uint8_t *arr, size_t bytes, bool big_endian) {
    int32_t out = 0;
    int i;

    arr += offset;
    if (big_endian) {
        for (i = 0; i < bytes; i++) {
            out = (out << 8) | arr[i];
        }
    } else {
        for (i = bytes-1; i >= 0; i--) {
            out = (out << 8) | arr[i];
        }
    }
    return out;
}

// Write 32-bit integer to data array, handling 'bytes' per sample
void sample_to_buffer(int32_t sample, int offset, uint8_t *arr, size_t bytes, bool big_endian) {

    int i;
    arr += offset;
    
    if (big_endian) {
        for (i = bytes-1; i >= 0; i--) {
            arr[i] = (sample & 0xff);
            sample >>= 8;
        }
    } else {
        for (i = 0; i < bytes; i++) {
            arr[i] = (sample & 0xff);
            sample >>= 8;
        }
    }
}

static void mixer_task(void *params) {
    // Find highest sample- and bitrate
    // Loop over sources, retrieve data, upsample and add to out_buf
    // If any source playing:
    //   render_audio
    // else:
    //   clear dma buffer

    source_ctx_t *ctx;
    uint8_t *out_buf = calloc(MIXER_BUF_LEN, sizeof(char));
    uint8_t *data;

    uint16_t sr,
             max_sample_rate;
    i2s_bits_per_sample_t bps,
                          max_bits_per_sample;

    size_t bytes_per_sample,
           max_bytes_per_sample;

    size_t bytes_read,
           max_bytes_read;
    
    bool dma_cleared = false;

    int32_t sampleI, sampleO;
    uint8_t channel;  // 0 = left, 1 = right
    int sources;  // Might be needed for normalization
    int i;
    for (;;) { 
        max_sample_rate = 0;
        max_bits_per_sample = 0;
        max_bytes_per_sample = 0;
        max_bytes_read = 0;

        // Find the highest sample- and bitrate
        sources = 0;
        while ((ctx = ctxs[sources++]) && ctx != NULL) {
            sr = ctx->buffer.format.sample_rate;
            bps = ctx->buffer.format.bits_per_sample;
            max_sample_rate = sr > max_sample_rate ? sr : max_sample_rate;
            if (bps > max_bits_per_sample) {
                max_bits_per_sample = bps;
                max_bytes_per_sample = bps > 16 ? 4 : bps/8;
            }
        }

        for (ctx = *ctxs; ctx != NULL; ctx++) {
            if (ctx->status != PLAYING)
                continue;
            bps = ctx->buffer.format.bits_per_sample;
            bytes_per_sample = bps > 16 ? 4 : bps/8;

            data = (uint8_t*) xRingbufferReceiveUpTo(ctx->buffer.data, &bytes_read,
                                                     portMAX_DELAY, MIXER_BUF_LEN);
            max_bytes_read = (bytes_read > max_bytes_read) ? bytes_read :
                                max_bytes_read;

            // For each sample (alternating between channels)
            // TODO: Implement option for non interleaved PCM (might already work)
            // TODO: Implement big-endian as well
            // TODO: Implement Upsampling
            // TODO: Write first buffer directly to out_buf instead of adding
            // TODO: Add support for more and less channels
            channel = 0;
            for (i = 0; i < bytes_read; i += bytes_per_sample) {
                if (i % 4 == 0)
                    channel = (channel+1) % 2;

                // Retrieve samples from in and output buffers
                sampleI = buffer_to_sample(i, data, bytes_per_sample, false);
                sampleO = buffer_to_sample(i, out_buf, max_bytes_per_sample, false);

                // Add them and return new sample to output buffer
                sampleO += sampleI;
                sample_to_buffer(sampleO, i, out_buf, max_bytes_per_sample, false);
            }

            vRingbufferReturnItem(ctx->buffer.data, data);
        }

        if (max_bytes_read > 0) {
            render_samples((uint8_t *)out_buf, max_bytes_read);
            memset(out_buf, 0, max_bytes_read);
            dma_cleared = false;
        } else {
            if (!dma_cleared) {
                renderer_clear_dma();
                dma_cleared = true;
            }
            // TODO: See if this can be done better, instead of delaying every time nothing has been done
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    free(out_buf);
}


void mixer_init() {
    ESP_LOGI(TAG, "Initializing");

    ctxs = source_return_ctxs();
    xTaskCreate(mixer_task, "Mixer", 2048, NULL, configMAX_PRIORITIES-1, s_mixer_task_handle);
}
