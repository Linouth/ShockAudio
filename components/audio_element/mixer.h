#ifndef MIXER_H
#define MIXER_H

#include <audio_element.h>
#include <io.h>

#define MIXER_MAX_INPUTS 4
#define MIXER_BUF_LEN 1024


audio_element_t *mixer_init(io_t *inputs[], size_t count);

#endif
