#ifndef SIMPLEAUDIO_H
#define SIMPLEAUDIO_H

#include <stdint.h>

#define GFXA_CHANNELS 4
#define GFXA_BUF_SIZE 256
#define GFXA_SAMPLE_RATE 16000

typedef int (*audio_stream_t)(int16_t *buf, int n, void *data);

extern int gfx_volume;

void gfxa_raw_stream(audio_stream_t fn);

int  gfxa_play(audio_stream_t fn, void *data, int channel);
void gfxa_stop(int channel);
void gfxa_wait(int channel, int ms);

void gfx_beep(int freq, int ms);

#endif
