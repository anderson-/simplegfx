#include "audio_engine.h"
#include <string.h>

/* ── Internal channel state ────────────────────────────────────────────── */

typedef struct {
  audio_fill_fn fn;
  void        *userdata;
  void       (*dtor)(void*);
  volatile bool  active;
  volatile bool  stopping;   /* fade-out solicitado */
  int  fade_pos;              /* progresso do fade (samples) */
  int  fade_len;              /* samples para fade-out */
  float volume;
  float pan;                  /* -1..1 */
} channel_t;

static struct {
  channel_t ch[GFXA_ENGINE_CHANNELS];
  volatile bool  running;    /* engine iniciado */
  int sample_rate;
} g_engine;

/* Buffer temporário estático (evita stack overflow na task de áudio).
 * Só usado dentro de gfxa_engine_mix, que roda sempre na mesma task. */
static int16_t s_tmp[1024];

/* ── Helpers ───────────────────────────────────────────────────────────── */

static int fade_samples(void) {
  return GFXA_ENGINE_FADE_MS * g_engine.sample_rate / 1000;
}

static void channel_done(int c) {
  channel_t *ch = &g_engine.ch[c];
  /* Sinaliza inativo PRIMEIRO (barreira) para o mixer parar de usar */
  ch->active = false;
  __sync_synchronize();

  ch->stopping = false;
  ch->fade_pos = 0;
  if (ch->dtor) {
    ch->dtor(ch->userdata);
    ch->dtor = NULL;
  }
  ch->fn = NULL;
  ch->userdata = NULL;
}

/* ── Public API ────────────────────────────────────────────────────────── */

void gfxa_engine_init(int sample_rate) {
  if (g_engine.running) return;
  memset(&g_engine, 0, sizeof(g_engine));
  g_engine.sample_rate = sample_rate > 0 ? sample_rate : GFXA_SAMPLE_RATE;
  g_engine.running = true;

  /* Inicializa fade_len para cada canal */
  int fl = fade_samples();
  for (int c = 0; c < GFXA_ENGINE_CHANNELS; c++)
    g_engine.ch[c].fade_len = fl;
}

static int find_free_channel(void) {
  for (int c = 0; c < GFXA_ENGINE_CHANNELS; c++)
    if (!g_engine.ch[c].active) return c;
  return -1;
}

int gfxa_engine_play(audio_fill_fn fn, void *userdata, void (*dtor)(void*)) {
  if (!g_engine.running) gfxa_engine_init(GFXA_SAMPLE_RATE);
  int c = find_free_channel();
  if (c < 0) return -1;

  channel_t *ch = &g_engine.ch[c];
  /* Preenche tudo ANTES de marcar active = true */
  ch->fn       = fn;
  ch->userdata = userdata;
  ch->dtor     = dtor;
  ch->stopping = false;
  ch->fade_pos = 0;
  ch->fade_len = fade_samples();
  ch->volume   = 1.0f;
  ch->pan      = 0.0f;
  __sync_synchronize();
  ch->active   = true;

  return c;
}

int gfxa_engine_play_on(int c, audio_fill_fn fn, void *userdata, void (*dtor)(void*)) {
  if (!g_engine.running) gfxa_engine_init(GFXA_SAMPLE_RATE);
  if (c < 0 || c >= GFXA_ENGINE_CHANNELS) return -1;

  /* Se o canal já está ativo, faz fade-out e espera */
  if (g_engine.ch[c].active) {
    g_engine.ch[c].stopping = true;
    int timeout = 200;
    while (g_engine.ch[c].active && --timeout > 0) {
      gfx_delay(1);
    }
  }

  channel_t *ch = &g_engine.ch[c];
  ch->fn       = fn;
  ch->userdata = userdata;
  ch->dtor     = dtor;
  ch->stopping = false;
  ch->fade_pos = 0;
  ch->fade_len = fade_samples();
  ch->volume   = 1.0f;
  ch->pan      = 0.0f;
  __sync_synchronize();
  ch->active   = true;

  return c;
}

void gfxa_engine_stop(int c) {
  if (c < 0 || c >= GFXA_ENGINE_CHANNELS) return;
  if (g_engine.ch[c].active)
    g_engine.ch[c].stopping = true;
}

void gfxa_engine_stop_all(void) {
  for (int c = 0; c < GFXA_ENGINE_CHANNELS; c++)
    gfxa_engine_stop(c);
}

void gfxa_engine_set_volume(int c, float vol) {
  if (c >= 0 && c < GFXA_ENGINE_CHANNELS)
    g_engine.ch[c].volume = vol < 0.0f ? 0.0f : vol > 1.0f ? 1.0f : vol;
}

void gfxa_engine_set_pan(int c, float pan) {
  if (c >= 0 && c < GFXA_ENGINE_CHANNELS)
    g_engine.ch[c].pan = pan < -1.0f ? -1.0f : pan > 1.0f ? 1.0f : pan;
}

bool gfxa_engine_is_playing(void) {
  for (int c = 0; c < GFXA_ENGINE_CHANNELS; c++)
    if (g_engine.ch[c].active) return true;
  return false;
}

bool gfxa_engine_is_channel_active(int c) {
  if (c < 0 || c >= GFXA_ENGINE_CHANNELS) return false;
  return g_engine.ch[c].active;
}

void gfxa_engine_wait(void) {
  while (gfxa_engine_is_playing()) {
    gfx_delay(1);
  }
}

/* ── Mixer ─────────────────────────────────────────────────────────────── */

int gfxa_engine_mix(int16_t *buf, int n) {
  memset(buf, 0, n * sizeof(int16_t));
  if (!g_engine.running) return n;

  for (int c = 0; c < GFXA_ENGINE_CHANNELS; c++) {
    channel_t *ch = &g_engine.ch[c];
    if (!ch->active) continue;

    /* Lê fn/userdata DEPOIS de confirmar active (barreira implícita
       porque ch->active é volatile e foi escrita antes de fn/userdata). */

    int max_n = n;
    if (max_n > (int)(sizeof(s_tmp)/sizeof(s_tmp[0])))
      max_n = (int)(sizeof(s_tmp)/sizeof(s_tmp[0]));

    int written = ch->fn(s_tmp, max_n, ch->userdata);

    if (written <= 0) {
      channel_done(c);
      continue;
    }

    /* Se stopping, aplica fade-out */
    if (ch->stopping) {
      int remain = ch->fade_len - ch->fade_pos;
      if (remain <= 0) {
        channel_done(c);
        continue;
      }
      int fade_n = written < remain ? written : remain;
      for (int i = 0; i < fade_n; i++) {
        float t = (float)(ch->fade_pos + i) / ch->fade_len;
        s_tmp[i] = (int16_t)(s_tmp[i] * (1.0f - t));
      }
      ch->fade_pos += fade_n;
      if (ch->fade_pos >= ch->fade_len) {
        channel_done(c);
        continue;
      }
      written = fade_n;
    }

    /* Mix com volume (mono para o buffer de saída) */
    float vol = ch->volume;
    if (vol != 1.0f) {
      for (int i = 0; i < written; i++)
        s_tmp[i] = (int16_t)(s_tmp[i] * vol);
    }

    for (int i = 0; i < written; i++) {
      int32_t s = (int32_t)buf[i] + (int32_t)s_tmp[i];
      if (s > 32767) s = 32767;
      else if (s < -32768) s = -32768;
      buf[i] = (int16_t)s;
    }
  }

  return n;
}

void gfxa_engine_deinit(void) {
  g_engine.running = false;
  for (int c = 0; c < GFXA_ENGINE_CHANNELS; c++) {
    if (g_engine.ch[c].active) channel_done(c);
  }
}
