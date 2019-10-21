#ifndef TONE_H
#define TONE_H

#include <stdint.h>
#include "audio.h"


void tone_task_start(struct audio_state *state);
void tone_gen();

#endif
