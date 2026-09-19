#ifndef PCM_INPUT_H
#define PCM_INPUT_H
#include <stdint.h>
int pcm_input_start(void);
void pcm_input_stop(void);
void pcm_input_read(int16_t *samples, int frames);
#endif
