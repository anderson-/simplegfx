#ifndef SIMPLEAUDIO_H
#define SIMPLEAUDIO_H

#include <stdint.h>

typedef int (*audio_stream_t)(int16_t *buf, int n, void *data);

extern volatile int _gfx_audio_playing;
extern int gfx_volume;

void gfxa_raw_stream(audio_stream_t fn);
void gfxa_stream(audio_stream_t fn, void *data, int sample_rate);
void gfxa_stream_wait(void);
void gfx_beep(int freq, int ms);

#endif
