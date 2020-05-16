#ifndef SOURCE_SDCARD_H
#define SOURCE_SDCARD_H

#include "audio_buffer.h"
#include "source.h"

#include <stdint.h>
#include <stddef.h>

#define SDREAD_BUF_SIZE 2048*4

/**
 * Play file from sdcard
 * Opens fd of file for the task to use
 * @param   filename: file to play
 */
int source_sdcard_play_file(char* filename);

/**
 * Initialize sdcard source
 * Initializes sd hardware, and allocates memory for the state structure.
 * Assigns play and pause functions to the structure. Also creates task.
 * @return  State structure
 */
void source_sdcard_init();

#endif
