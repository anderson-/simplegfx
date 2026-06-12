#include "simpleaudio.h"
#include "simplegfx.h"
#include <string.h>
#include <stdlib.h>

int gfx_volume = 1;

typedef struct {
  int16_t buf[GFXA_BUF_SIZE];
  audio_stream_t fn;
  void *data;
  int playing;
} chan_t;

static chan_t *chans[GFXA_CHANNELS];

static void apply_fade_out(int16_t *buf, int n) {
  for (int i = 0; i < n; i++)
    buf[i] = (int16_t)(buf[i] * (n - 1 - i) / n);
}

static int _mix_fill(int16_t *out, int n, void *user) {
  for (int i = 0; i < n; i++) out[i] = 0;
  for (int c = 0; c < GFXA_CHANNELS; c++) {
    chan_t *ch = chans[c];
    if (!ch) continue;

    for (int i = 0; i < n; i++)
      out[i] = (int16_t)(out[i] + ch->buf[i]);

    if (!ch->fn) {
      free(ch);
      chans[c] = NULL;
      continue;
    }

    if (ch->playing == 0) {
      int r = ch->fn(ch->buf, n, ch->data);
      if (r <= 0) {
        apply_fade_out(ch->buf, n);
        ch->fn = NULL;
      }
    } else {
      ch->playing = 0;
    }
  }

  return n;
}

int gfxa_play(audio_stream_t fn, void *data, int channel) {
  static int started = 0;
  if (!fn) return -1;

  int c = channel;
  if (c < 0)
    for (int i = 0; i < GFXA_CHANNELS; i++)
      if (!chans[i]) { c = i; break; }
  if (c < 0 || c >= GFXA_CHANNELS) return -1;

  if (!chans[c]) {
    chans[c] = (chan_t *)malloc(sizeof(chan_t));
    if (!chans[c]) return -1;
    memset(chans[c], 0, sizeof(chan_t));
    chans[c]->fn = fn;
    chans[c]->data = data;
    fn(chans[c]->buf, GFXA_BUF_SIZE, data);
    for (int i = 0; i < GFXA_BUF_SIZE; i++)
      chans[c]->buf[i] = (int16_t)(chans[c]->buf[i] * i / GFXA_BUF_SIZE);
    chans[c]->playing = 1;
  } else {
    chan_t *ch = chans[c];
    int16_t tmp[GFXA_BUF_SIZE];
    memcpy(tmp, ch->buf, GFXA_BUF_SIZE * sizeof(int16_t));
    ch->fn = fn;
    ch->data = data;
    fn(ch->buf, GFXA_BUF_SIZE, data);
    apply_fade_out(tmp, GFXA_BUF_SIZE);
    for (int i = 0; i < GFXA_BUF_SIZE; i++)
      ch->buf[i] = (int16_t)((int32_t)tmp[i] + (int32_t)ch->buf[i] * i / GFXA_BUF_SIZE);
    ch->playing = 1;
  }

  if (started) return c;
  started = 1;
  gfxa_raw_stream(_mix_fill);
  return c;
}

static void chan_stop(chan_t *ch) {
  apply_fade_out(ch->buf, GFXA_BUF_SIZE);
  ch->fn = NULL;
}

void gfxa_stop(int channel) {
  if (channel < 0) {
    for (int c = 0; c < GFXA_CHANNELS; c++)
      if (chans[c]) chan_stop(chans[c]);
    return;
  }
  if (channel >= GFXA_CHANNELS) return;
  if (chans[channel]) chan_stop(chans[channel]);
}

void gfxa_wait(int channel, int ms) {
  while (ms) {
    int any = 0;
    if (channel < 0) {
      for (int c = 0; c < GFXA_CHANNELS; c++)
        if (chans[c]) { any = 1; break; }
    } else if (channel < GFXA_CHANNELS && chans[channel]) any = 1;
    if (!any) break;
    gfx_delay(1);
    ms--;
  }
  //gfx_delay(GFXA_BUF_SIZE * 1000 / GFXA_SAMPLE_RATE);
}

typedef struct { int freq, sr, pos, len; } beep_t;

static int _beep_fill(int16_t *buf, int n, void *user) {
  beep_t *b = user;
  int i = 0;
  while (i < n && b->pos < b->len) {
    uint32_t ph = (uint32_t)(((uint64_t)b->freq * b->pos * 65536) / b->sr);
    buf[i++] = (int16_t)((gfx_fast_isin((int32_t)ph) * 32767) >> 15);
    b->pos++;
  }
  return i;
}

void gfx_beep(int freq, int ms) {
  int sr = GFXA_SAMPLE_RATE;
  int n = sr * ms / 1000;
  if (n < 2 * GFXA_BUF_SIZE) n = 2 * GFXA_BUF_SIZE;
  beep_t state = {freq, sr, 0, n};
  gfxa_wait(gfxa_play(_beep_fill, &state, -1), ms);
}
