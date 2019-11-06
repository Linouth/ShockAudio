#include <stdint.h>
#include "esp_log.h"

#include "pcm.h"

static const char* TAG = "PCM";


uint8_t *upsample(uint8_t *data, size_t data_len, size_t *out_len, pcm_format pcm, int to_rate) {
    int bytes_per_sample = pcm.bits_per_sample <= 16 ? (pcm.bits_per_sample/8) : 4;
    int sample_mult = pcm.channels * bytes_per_sample;  // From sample_count to byte count
    int sample_count = data_len / (sample_mult);

    // TODO: Make more accurate for non rational sample rates (e.g. 8000 -> 44100)
    int rate_mult = to_rate / pcm.sample_rate;
    
    *out_len = data_len * rate_mult;
    uint8_t *out = calloc(*out_len, sizeof(uint8_t));
    if (!out) {
        ESP_LOGE(TAG, "Could not allocate memory");
        exit(1);
    }

    int last_index = 0;
    int i, j;
    for (i = 0; i < sample_count*rate_mult; i++) {
        // For every sample in the new signal
        
        for (j = 0; j < sample_mult; j++) {
            if (i % rate_mult == 0) {
                // Copy bytes over
                out[i*sample_mult + j] = data[i/rate_mult * sample_mult + j];
                last_index = i/rate_mult;
            } else {
                // Write previous sample for a much cleaner output signal
                // TODO: Even beter would be to take some average between the last and the next sample
                out[i*sample_mult + j] = data[last_index * sample_mult + j];
            }
        }
    }

    return out;
}
