#ifndef SIMPLEAUDIO_H
#define SIMPLEAUDIO_H

#include <stdint.h>

#define GFXA_CHANNELS 4
#define GFXA_BUF_SIZE 480
#define GFXA_SAMPLE_RATE 24000

typedef int (*audio_stream_t)(int16_t *buf, int n, void *data);

typedef struct chan_t {
  int16_t buf[GFXA_BUF_SIZE];
  audio_stream_t fn;
  void *data;
  int playing;
  float vol;
  void *ctrl_data;
  void (*ctrl)(struct chan_t *chan, void *ctrl_data);
} chan_t;

typedef void (*audio_ctrl_t)(chan_t *chan, void *ctrl_data);

extern float gfx_volume;

void gfxa_raw_stream(audio_stream_t fn);

int  gfxa_play(audio_stream_t fn, void *data, int channel);
void gfxa_stop(int channel);
void gfxa_wait(int channel, int ms);

void gfxa_set_volume(int channel, float vol);
void gfxa_set_ctrl(int channel, audio_ctrl_t fn, void *ctrl_data);

void gfxa_chan_play(chan_t *chan, audio_stream_t fn, void *data);
void gfxa_chan_stop(chan_t *chan);

void gfx_beep(int freq, int ms);

#endif
