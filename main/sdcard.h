#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>
#include <stddef.h>

void sd_init();
void sd_close();
int sd_open_file(char* filename);
size_t sd_get_data(uint8_t* buf, int read_len);

#endif
