#ifndef SIMPLEAUDIO_H
#define SIMPLEAUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <simplegfx.h>

extern double volume;

int audio_setup(void);
void audio_cleanup(void);
void beep(int freq, int ms);
void audio_callback(void * userdata, uint8_t * stream, int len);

#ifdef __cplusplus
}
#endif

#endif