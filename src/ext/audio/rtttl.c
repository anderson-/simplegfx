#include "rtttl.h"
#include "simplegfx.h"
#include "audio_engine.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define GFXA_RTTTL_MAX_NOTES 128
#define GFXA_RTTTL_FADE_MS   16

static void (*_rtttl_cb)(bool) = NULL;

void gfxa_rtttl_set_callback(void (*cb)(bool)) {
  _rtttl_cb = cb;
}

/* ── Note data ─────────────────────────────────────────────────────────── */

typedef struct {
  int freq;   /* 0 = pausa */
  int ms;     /* duração em ms */
} rtttl_note_t;

typedef struct {
  rtttl_note_t notes[GFXA_RTTTL_MAX_NOTES];
  int  count;
  int  pos;          /* nota atual */
  int  sample_pos;   /* sample dentro da nota atual */
  int  sample_rate;
  int  fade_samples;
  void (*cb)(bool);  /* callback per-note (pode ser NULL) */
} rtttl_song_t;

/* ── Fill function ─────────────────────────────────────────────────────── */

static int rtttl_fill(int16_t *buf, int n, void *user) {
  rtttl_song_t *song = (rtttl_song_t *)user;
  int out = 0;

  while (out < n && song->pos < song->count) {
    int freq  = song->notes[song->pos].freq;
    int ms    = song->notes[song->pos].ms;
    int total = ms * song->sample_rate / 1000;    /* samples desta nota */
    int remaining = total - song->sample_pos;
    int todo = (n - out) < remaining ? (n - out) : remaining;

    /* Gera samples para esta nota */
    for (int i = 0; i < todo; i++) {
      int local = song->sample_pos + i;
      float amp = 16000.0f;

      /* Fade in/out de 16ms na nota */
      if (local < song->fade_samples)
        amp *= (float)local / song->fade_samples;
      else if (total - local - 1 < song->fade_samples)
        amp *= (float)(total - local - 1) / song->fade_samples;

      if (freq > 0) {
        float t = 2.0f * (float)M_PI * (float)freq * (float)local / (float)song->sample_rate;
        buf[out++] = (int16_t)(sinf(t) * amp);
      } else {
        buf[out++] = 0;  /* pausa */
      }
    }

    song->sample_pos += todo;

    /* Avança para próxima nota se esta terminou */
    if (song->sample_pos >= total) {
      /* Callback: nota anterior terminou (false) */
      if (song->cb) song->cb(false);
      if (_rtttl_cb) _rtttl_cb(false);
      song->sample_pos = 0;
      song->pos++;
      /* Callback: nova nota começou (true) — se não for a última posição
         que já vai chamar false no dtor) */
      if (song->pos < song->count && song->notes[song->pos].freq > 0) {
        if (song->cb) song->cb(true);
        if (_rtttl_cb) _rtttl_cb(true);
      }
    }
  }

  return out;
}

/* ── dtor: chamado pelo engine quando o stream termina ─────────────────── */

static void rtttl_dtor(void *user) {
  rtttl_song_t *song = (rtttl_song_t *)user;
  if (song->cb) song->cb(false);
  if (_rtttl_cb) _rtttl_cb(false);
  free(song);
}

/* ── Helpers de parse ──────────────────────────────────────────────────── */

static int note_index(char c) {
  switch ((char)tolower((unsigned char)c)) {
    case 'c': return 0;
    case 'd': return 2;
    case 'e': return 4;
    case 'f': return 5;
    case 'g': return 7;
    case 'a': return 9;
    case 'b': return 11;
    default: return -1;
  }
}

static int note_freq(int octave, int note) {
  int midi = (octave + 1) * 12 + note;
  return (int)(440.0f * powf(2.0f, (float)(midi - 69) / 12.0f) + 0.5f);
}

static const char *skip_number(const char *p, int *value) {
  int n = 0;
  bool any = false;
  while (*p >= '0' && *p <= '9') {
    n = n * 10 + (*p - '0');
    ++p;
    any = true;
  }
  if (any) *value = n;
  return p;
}

/* ── Parser: preenche rtttl_song_t a partir da string RTTTL ───────────── */

static int parse_rtttl(const char *rtttl, rtttl_song_t *song) {
  if (!rtttl || !rtttl[0]) return 1;

  const char *p = strchr(rtttl, ':');
  if (!p) return 1;
  ++p;

  int def_dur = 4;
  int def_oct = 6;
  int bpm = 63;

  const char *notes = strchr(p, ':');
  if (notes) {
    char header[96];
    size_t len = (size_t)(notes - p);
    if (len >= sizeof(header)) len = sizeof(header) - 1;
    memcpy(header, p, len);
    header[len] = '\0';

    char *tok = strtok(header, ",");
    while (tok) {
      if (tok[0] == 'd' && tok[1] == '=') def_dur = atoi(tok + 2);
      if (tok[0] == 'o' && tok[1] == '=') def_oct = atoi(tok + 2);
      if (tok[0] == 'b' && tok[1] == '=') bpm = atoi(tok + 2);
      tok = strtok(NULL, ",");
    }
    p = notes + 1;
  }

  int whole_ms = (60 * 1000 / (bpm > 0 ? bpm : 63)) * 4;
  const char *start = p;
  int note_idx = 0;

  while (*p && (p - start) < 512 && note_idx < GFXA_RTTTL_MAX_NOTES) {
    while ((*p == ' ' || *p == ',') && (p - start) < 512) ++p;
    if (!*p) break;

    int duration = def_dur;
    int octave = def_oct;
    p = skip_number(p, &duration);

    char c = *p ? *p++ : 'p';
    bool sharp = false;
    bool dotted = false;

    if (*p == '#') { sharp = true; ++p; }
    if (*p == '.') { dotted = true; ++p; }
    if (*p >= '0' && *p <= '9') octave = *p++ - '0';
    if (*p == '.') { dotted = true; ++p; }

    int ms = whole_ms / (duration > 0 ? duration : def_dur);
    if (dotted) ms += ms / 2;

    int ni = note_index(c);
    if (ni < 0) {
      song->notes[note_idx].freq = 0;  /* pausa */
    } else {
      if (sharp) ++ni;
      song->notes[note_idx].freq = note_freq(octave, ni);
    }
    song->notes[note_idx].ms = ms;

    note_idx++;
    if (*p == ',') ++p;
  }

  song->count = note_idx;
  if ((p - start) >= 512 && note_idx >= GFXA_RTTTL_MAX_NOTES)
    return 1;  /* truncou */

  return 0;  /* OK */
}

/* ── Play: sempre async via engine ─────────────────────────────────────── */

int gfxa_rtttl_play(const char *rtttl, void (*cb)(bool)) {
  rtttl_song_t *song = (rtttl_song_t *)malloc(sizeof(*song));
  if (!song) return 1;

  memset(song, 0, sizeof(*song));
  song->sample_rate  = 16000;
  song->fade_samples = GFXA_RTTTL_FADE_MS * song->sample_rate / 1000;
  song->cb = cb;

  if (parse_rtttl(rtttl, song) != 0) {
    free(song);
    return 1;
  }

  /* Callback da primeira nota */
  if (song->count > 0 && song->notes[0].freq > 0) {
    if (song->cb) song->cb(true);
    if (_rtttl_cb) _rtttl_cb(true);
  }

  gfxa_stream(rtttl_fill, song, rtttl_dtor, song->sample_rate, GFXA_ASYNC);
  return 0;
}
