#include "simpleaudio.h"
#include "simplegfx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int gfx_volume = 1;

typedef struct {
  int16_t buf[GFXA_BUF_SIZE];
  audio_stream_t fn;
  void *data;
  int playing;
  int vol;
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

    int v = ch->vol;
    if (v <= 1) {
      for (int i = 0; i < n; i++)
        out[i] = (int16_t)(out[i] + ch->buf[i]);
    } else {
      for (int i = 0; i < n; i++)
        out[i] = (int16_t)(out[i] + ch->buf[i] / v);
    }

    if (!ch->fn) {
      free(ch);
      chans[c] = NULL;
      continue;
    }

    if (ch->playing) {
      ch->playing = 0;
    }

    int r = ch->fn(ch->buf, n, ch->data);
    if (r < n) {
      apply_fade_out(ch->buf, r);
      for (int i = r; i < n; i++) ch->buf[i] = 0;
      ch->fn = NULL;
    }
  }

  if (gfx_volume > 1) {
    for (int i = 0; i < n; i++)
      out[i] /= gfx_volume;
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

  chan_t *ch = chans[c];
  if (!ch) {
    ch = chans[c] = (chan_t *)malloc(sizeof(chan_t));
    if (!ch) return -1;
    memset(ch, 0, sizeof(chan_t));
    ch->fn = fn;
    ch->data = data;
    ch->vol = 1;
    fn(ch->buf, GFXA_BUF_SIZE, data);
    for (int i = 0; i < GFXA_BUF_SIZE; i++)
      ch->buf[i] = (int16_t)((int32_t)ch->buf[i] * i / GFXA_BUF_SIZE);
  } else {
    int16_t tmp[GFXA_BUF_SIZE];
    memcpy(tmp, ch->buf, GFXA_BUF_SIZE * sizeof(int16_t));
    ch->fn = fn;
    ch->data = data;
    fn(ch->buf, GFXA_BUF_SIZE, data);
    for (int i = 0; i < GFXA_BUF_SIZE; i++)
      ch->buf[i] = (int16_t)((int32_t)tmp[i] * (GFXA_BUF_SIZE - i) / GFXA_BUF_SIZE
                           + (int32_t)ch->buf[i] * i / GFXA_BUF_SIZE);
  }
  ch->playing = 1;

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
  int waited = 0;
  while (ms > 0) {
    int any = 0;
    if (channel < 0) {
      for (int c = 0; c < GFXA_CHANNELS; c++)
        if (chans[c]) { any = 1; break; }
    } else if (channel < GFXA_CHANNELS && chans[channel]) {
      any = 1;
    }
    if (!any) break;
    gfx_delay(1);
    ms--;
    waited = 1;
  }
  if (waited) gfx_delay(GFXA_BUF_SIZE * 1000 / GFXA_SAMPLE_RATE);
}

void gfxa_set_volume(int channel, int vol) {
  if (vol < 0) vol = 0;
  if (channel < 0) {
    gfx_volume = vol;
    return;
  }
  if (channel >= GFXA_CHANNELS) return;
  if (chans[channel]) chans[channel]->vol = vol;
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
  beep_t *state = malloc(sizeof(beep_t));
  *state = (beep_t){freq, sr, 0, n};
  gfxa_wait(gfxa_play(_beep_fill, state, -1), ms);
  free(state);
}
