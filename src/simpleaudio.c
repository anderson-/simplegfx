#include "simpleaudio.h"
#include "simplegfx.h"

int gfx_volume = 1;
volatile int _gfx_audio_playing = 0;

static audio_stream_t _raw_fn;
static void *_raw_user;

static int _vol_fill(int16_t *buf, int n, void *user) {
  (void)user;
  if (!_raw_fn) return 0;
  int r = _raw_fn(buf, n, _raw_user);
  if (r <= 0) { _gfx_audio_playing = 0; return r; }
  if (gfx_volume > 1)
    for (int i = 0; i < r; i++) buf[i] /= gfx_volume;
  return r;
}

void gfxa_stream(audio_stream_t fn, void *data, int sample_rate) {
  _raw_fn = fn;
  _raw_user = data;
  _gfx_audio_playing = 1;
  gfxa_raw_stream(_vol_fill);
  while (_gfx_audio_playing) gfx_delay(1);
  _raw_fn = NULL;
  _raw_user = NULL;
}

void gfxa_stream_wait(void) {
  while (_gfx_audio_playing) gfx_delay(1);
}

typedef struct { int freq, sr, pos, len, fade; } beep_t;

static int _beep_fill(int16_t *buf, int n, void *user) {
  beep_t *b = user;
  int i = 0;
  while (i < n && b->pos < b->len) {
    int amp = 32767;
    if (b->pos < b->fade)
      amp = amp * b->pos / b->fade;
    else if (b->len - b->pos - 1 < b->fade)
      amp = amp * (b->len - b->pos - 1) / b->fade;
    uint32_t ph = (uint32_t)(((uint64_t)b->freq * b->pos * 65536) / b->sr);
    buf[i++] = (int16_t)((gfx_fast_isin((int32_t)ph) * amp) >> 15);
    b->pos++;
  }
  return i;
}

void gfx_beep(int freq, int ms) {
  int sr = 16000;
  int n = sr * ms / 1000;
  if (n < 512) n = 512;
  int fade = sr * 16 / 1000;
  if (fade > n / 2) fade = n / 2;
  beep_t state = {freq, sr, 0, n, fade};
  gfxa_stream(_beep_fill, &state, sr);
}
