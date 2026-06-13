#include "simpleaudio.h"
#include "simplegfx.h"
#include <stdlib.h>

float gfx_volume = 1;

typedef struct {
  int16_t buf[GFXA_BUF_SIZE];
  audio_stream_t fn;
  void *data;
  audio_ctrl_t ctrl;
  void *ctrl_data;
  int playing;
  float vol;
} chan_t;

static chan_t chans[GFXA_CHANNELS];

static void apply_fade_out(int16_t *buf, int n) {
  for (int i = 0; i < n; i++)
    buf[i] = (int16_t)(buf[i] * (n - 1 - i) / n);
}

static int _mix_fill(int16_t *out, int n, void *user) {
  for (int i = 0; i < n; i++) {
    int32_t sum = 0;
    for (int c = 0; c < GFXA_CHANNELS; c++) {
      chan_t *ch = &chans[c];
      if (!ch->playing) continue;
      int v = ch->vol;
      if (v <= 1 || v < 0)
        sum += ch->buf[i];
      else
        sum += ch->buf[i] * v;
    }
    if (gfx_volume > 1) sum *= gfx_volume;
    if (sum > 32767) sum = 32767;
    else if (sum < -32768) sum = -32768;
    out[i] = (int16_t)sum;
  }
  for (int c = 0; c < GFXA_CHANNELS; c++) {
    chan_t *ch = &chans[c];
    if (!ch->playing) continue;
    if (!ch->fn) {
      for (int i = 0; i < GFXA_BUF_SIZE; i++) ch->buf[i] = 0;
      ch->data = NULL;
      ch->playing = 0;
      continue;
    }
    if (ch->ctrl) ch->ctrl(ch->ctrl_data, ch->data);
    int r = ch->fn(ch->buf, n, ch->data);
    if (r < n) {
      apply_fade_out(ch->buf, r);
      for (int i = r; i < n; i++) ch->buf[i] = 0;
      ch->fn(NULL, 0, ch->data); // cleanup
      ch->fn = NULL;
    }
  }
  return n;
}

int gfxa_play(audio_stream_t fn, void *data, int channel) {
  static int started = 0;
  if (!fn) return -1;

  int c = channel;
  if (c < 0) {
    for (int i = 0; i < GFXA_CHANNELS; i++) {
      if (!chans[i].playing) {
        c = i;
        break;
      }
    }
  }

  if (c < 0 || c >= GFXA_CHANNELS) return -1;
  chan_t *ch = &chans[c];
  if (!ch->playing) {
    for (int i = 0; i < GFXA_BUF_SIZE; i++) ch->buf[i] = 0;
    ch->playing = 1;
    ch->fn = fn;
    ch->data = data;
    if (channel < 0) ch->vol = 1;
    int r = fn(ch->buf, GFXA_BUF_SIZE, data);
    if (r < GFXA_BUF_SIZE) {
      for (int i = r; i < GFXA_BUF_SIZE; i++)
        ch->buf[i] = 0;
      ch->fn = NULL;
    }
    for (int i = 0; i < GFXA_BUF_SIZE; i++) {
      ch->buf[i] = (int16_t)(
        (int32_t)ch->buf[i] * i / GFXA_BUF_SIZE
      );
    }
  } else {
    int16_t tmp[GFXA_BUF_SIZE];
    memcpy(tmp, ch->buf, GFXA_BUF_SIZE * sizeof(int16_t));
    ch->fn = fn;
    ch->data = data;
    int r = fn(ch->buf, GFXA_BUF_SIZE, data);
    if (r < GFXA_BUF_SIZE) {
      for (int i = r; i < GFXA_BUF_SIZE; i++)
        ch->buf[i] = 0;
      ch->fn = NULL;
    }
    for (int i = 0; i < GFXA_BUF_SIZE; i++) {
      ch->buf[i] = (int16_t)(
        (int32_t)tmp[i] * (GFXA_BUF_SIZE - i) / GFXA_BUF_SIZE +
        (int32_t)ch->buf[i] * i / GFXA_BUF_SIZE
      );
    }
  }

  if (started) return c;
  started = 1;
  gfxa_raw_stream(_mix_fill);
  for (int c = 0; c < GFXA_CHANNELS; c++) {
    chans[c].vol = 1;
    chans[c].ctrl = NULL;
    chans[c].ctrl_data = NULL;
  }
  return c;
}

static void chan_stop(chan_t *ch) {
  if (!ch->playing) return;
  apply_fade_out(ch->buf, GFXA_BUF_SIZE);
  ch->fn = NULL;
}

void gfxa_stop(int channel) {
  if (channel < 0) {
    for (int c = 0; c < GFXA_CHANNELS; c++)
      chan_stop(&chans[c]);
    return;
  }
  if (channel >= GFXA_CHANNELS) return;
  chan_stop(&chans[channel]);
}

void gfxa_wait(int channel, int ms) {
  int waited = 0;
  while (ms > 0) {
    int any = 0;
    if (channel < 0) {
      for (int c = 0; c < GFXA_CHANNELS; c++) {
        if (chans[c].playing) { any = 1; break; }
      }
    } else if (channel < GFXA_CHANNELS && chans[channel].playing) {
      any = 1;
    }
    if (!any) break;
    gfx_delay(1);
    ms--;
    waited = 1;
  }
  if (waited) gfx_delay(GFXA_BUF_SIZE * 1000 / GFXA_SAMPLE_RATE); // TODO: use real buffer fill
}

void gfxa_set_volume(int channel, float vol) {
  if (vol < 0) vol = 0;
  if (channel < 0) {
    gfx_volume = vol;
    return;
  }
  if (channel >= GFXA_CHANNELS) return;
  if (chans[channel].playing)
    chans[channel].vol = vol;
}

void gfxa_set_ctrl(int channel, audio_ctrl_t fn, void *ctrl_data) {
  if (channel < 0) {
    for (int i = 0; i < GFXA_CHANNELS; i++) {
      chans[i].ctrl = fn;
      chans[i].ctrl_data = ctrl_data;
    }
    return;
  }
  if (channel >= GFXA_CHANNELS) return;
  chans[channel].ctrl = fn;
  chans[channel].ctrl_data = ctrl_data;
}

typedef struct { int freq, sr, pos, len; } beep_t;

static int _beep_fill(int16_t *buf, int n, void *user) {
  if (!buf) return -1; // cleanup
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
  beep_t state = { freq, sr, 0, n };
  int ch = gfxa_play(_beep_fill, &state, -1);
  if (ch >= 0)
    gfxa_wait(ch, ms);
}
