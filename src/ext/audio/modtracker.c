#include "modtracker.h"
#include "simplegfx.h"
#include "audio_engine.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  O sequenciador roda DENTRO do callback de áudio (msm_fill), com precisão
 *  de sample: um tick dura 2.5/BPM segundos (padrão ProTracker), um row dura
 *  `speed` ticks. As 4 vozes sfxr são mixadas internamente e o player ocupa
 *  um único canal do engine — sem malloc, sem espera bloqueante e sem brigar
 *  com outros sons do app.
 * ══════════════════════════════════════════════════════════════════════════ */

#define MSM_DECLICK_SAMPLES   96    /* fade do retrigger (~6 ms @16 kHz)  */
#define MSM_RELEASE_SAMPLES 1200    /* fade do NOTE_OFF  (~75 ms)         */
#define MSM_PORTA_SCALE     1.0f    /* unidades de período por tick/param */
#define MSM_MIX_CHUNK        128    /* granularidade da mixagem           */
#define MSM_DYING_VOICES       2    /* caudas por canal para evitar clicks */
#define MSM_Q15           32768

typedef struct {
  /* Voz atual e voz anterior em fade-out (declick/release) */
  struct sfxr_state *voice;
  struct sfxr_state *dying[MSM_DYING_VOICES];
  int   dying_len[MSM_DYING_VOICES];   /* tamanho total do fade               */
  int   dying_left[MSM_DYING_VOICES];  /* samples restantes                   */

  int   note;                 /* nota MIDI atual (-1 = nenhuma)          */
  int   instrument;           /* instrumento atual (-1 = nenhum)         */
  int   volume;               /* 0..64                                   */
  bool  muted;

  float period_base;          /* período da nota (com slides acumulados) */
  float period_out;           /* período efetivo (base + vibrato)        */

  /* Memória de efeitos */
  float   porta_target;       /* período alvo do 3xx (0 = nenhum)        */
  uint8_t porta_speed;        /* memória 3xx                             */
  uint8_t slide_speed;        /* memória 1xx/2xx                         */
  uint8_t vol_slide;          /* memória Axy                             */
  uint8_t vib_param;          /* memória 4xy                             */
  uint8_t trem_param;         /* memória 7xy                             */
  int     vib_phase;
  int     trem_phase;

  /* Célula do row atual (cópia estável para os ticks) */
  msm_cell_t cell;
  int   delayed_note;         /* nota pendente do EDx (-1 = nenhuma)     */

  /* Jam: nota injetada pela UI (thread-safe via flag volátil) */
  volatile uint8_t jam_note;
  volatile uint8_t jam_inst;
  volatile uint8_t jam_flag;
} msm_voice_t;

struct msm_player {
  msm_song_t *song;
  volatile int  playing;
  volatile int  edit_lock;
  /* Transporte é pedido pela UI e executado na thread de áudio (fill),
   * para nunca destruir uma voz que o mixer esteja lendo. */
  volatile int  restart_req;     /* re-inicia na posição pedida */
  volatile int  silence_req;     /* solta todas as vozes        */

  int   speed;                /* ticks por row */
  int   bpm;
  volatile int position;      /* posição na sequência */
  volatile int row;
  volatile int tick;
  int   samples_per_tick;
  int   samples_left;         /* até o próximo tick */

  bool  loop_pattern;
  bool  break_request;
  int   break_row;
  int   jump_position;        /* -1 = nenhum */

  float global_vol;
  int   global_vol_q15;

  msm_voice_t v[MSM_CHANNELS];
  int16_t tmp[MSM_MIX_CHUNK];
  uint8_t inst_cache_valid[MSM_MAX_INSTRUMENTS];
  uint8_t inst_cache_packed[MSM_MAX_INSTRUMENTS][GFXA_SFXR_PARAM_COUNT];
  float inst_cache_params[MSM_MAX_INSTRUMENTS][GFXA_SFXR_PARAM_COUNT];
};

/* ─── Helpers ──────────────────────────────────────────────────────────── */

static float midi_freq[120];
static int midi_freq_ready = 0;

static void init_midi_freq(void) {
  if (midi_freq_ready) return;
  for (int i = 0; i < 120; i++)
    midi_freq[i] = 440.0f * powf(2.0f, (float)(i - 69) / 12.0f);
  midi_freq_ready = 1;
}

float msm_midi_to_freq(int midi_note) {
  init_midi_freq();
  if (midi_note < 0) midi_note = 0;
  if (midi_note > 119) midi_note = 119;
  return midi_freq[midi_note];
}

/* 2^(-n/12) para arpejo (0..15 semitons acima = período menor) */
static const float pow2_down[16] = {
  1.0000000f, 0.9438743f, 0.8908987f, 0.8408964f,
  0.7937005f, 0.7491535f, 0.7071068f, 0.6674199f,
  0.6299605f, 0.5946036f, 0.5612310f, 0.5297315f,
  0.5000000f, 0.4719372f, 0.4454494f, 0.4204482f,
};

static int current_pattern(const struct msm_player *p) {
  if (p->position >= p->song->header.sequence_length) return 0;
  int pat = p->song->sequence[p->position];
  if (pat >= p->song->header.pattern_count) return 0;
  return pat;
}

static int float_to_q15(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 1.0f) return MSM_Q15;
  return (int)(v * (float)MSM_Q15 + 0.5f);
}

static void update_tick_len(struct msm_player *p) {
  int bpm = p->bpm > 0 ? p->bpm : MSM_BPM_DEFAULT;
  /* tick = 2.5/bpm s  →  samples = sr * 5 / (bpm * 2) */
  p->samples_per_tick = GFXA_SAMPLE_RATE * 5 / (bpm * 2);
  if (p->samples_per_tick < 16) p->samples_per_tick = 16;
}

/* Move a voz atual para o slot de fade-out. */
static void voice_release(msm_voice_t *c, int fade_len) {
  if (!c->voice) { c->note = -1; return; }

  int slot = -1;
  for (int i = 0; i < MSM_DYING_VOICES; i++) {
    if (!c->dying[i]) { slot = i; break; }
  }
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < MSM_DYING_VOICES; i++)
      if (c->dying_left[i] < c->dying_left[slot]) slot = i;
    gfxa_sfxr_destroy(c->dying[slot]);
  }

  c->dying[slot]      = c->voice;
  c->dying_len[slot]  = fade_len;
  c->dying_left[slot] = fade_len;
  c->voice            = NULL;
  c->note             = -1;
}

static void voice_free_all(msm_voice_t *c) {
  if (c->voice) { gfxa_sfxr_destroy(c->voice); c->voice = NULL; }
  for (int i = 0; i < MSM_DYING_VOICES; i++) {
    if (c->dying[i]) {
      gfxa_sfxr_destroy(c->dying[i]);
      c->dying[i] = NULL;
    }
    c->dying_left[i] = 0;
  }
  c->note = -1;
}

static const float *instrument_params(struct msm_player *p, int inst) {
  const uint8_t *packed = p->song->instruments[inst].params;
  if (!p->inst_cache_valid[inst] ||
      memcmp(p->inst_cache_packed[inst], packed, GFXA_SFXR_PARAM_COUNT) != 0) {
    memcpy(p->inst_cache_packed[inst], packed, GFXA_SFXR_PARAM_COUNT);
    gfxa_sfxr_unpack(packed, p->inst_cache_params[inst]);
    p->inst_cache_valid[inst] = 1;
  }
  return p->inst_cache_params[inst];
}

/* Dispara uma nota na voz (roda na thread de áudio). */
static void voice_note_on(struct msm_player *p, int ch, int note, int inst) {
  msm_voice_t *c = &p->v[ch];
  const msm_song_t *song = p->song;

  if (inst >= 0 && inst < song->header.instrument_count)
    c->instrument = inst;
  if (c->instrument < 0 || c->instrument >= song->header.instrument_count)
    return;

  const float *params = instrument_params(p, c->instrument);
  struct sfxr_state *s = gfxa_sfxr_create(params);
  if (!s) return;

  gfxa_sfxr_set_freq(s, msm_midi_to_freq(note));

  if (c->voice) voice_release(c, MSM_DECLICK_SAMPLES);
  c->voice       = s;
  c->note        = note;
  c->period_base = gfxa_sfxr_get_period(s);
  c->period_out  = c->period_base;
  c->vib_phase   = 0;
  c->trem_phase  = 0;
}

static float note_period(const msm_voice_t *c, int note) {
  if (!c->voice) return 0.0f;
  return gfxa_sfxr_freq_to_period(c->voice, msm_midi_to_freq(note));
}

/* ─── Row (tick 0) ─────────────────────────────────────────────────────── */

static void trigger_cell(struct msm_player *p, int ch) {
  msm_voice_t *c = &p->v[ch];
  const msm_cell_t *cell = &c->cell;
  int note = cell->note;

  if (note == MSM_NOTE_CUT) { voice_release(c, MSM_DECLICK_SAMPLES); return; }
  if (note == MSM_NOTE_OFF) { voice_release(c, MSM_RELEASE_SAMPLES); return; }
  if (note >= 120) return;

  if (cell->effect == MSM_FX_TONE_PORTA && c->voice && c->note >= 0) {
    /* 3xx com nota: não retriga, só define o alvo do slide */
    c->porta_target = note_period(c, note);
    if (cell->instrument != MSM_INST_NONE &&
        cell->instrument < p->song->header.instrument_count)
      c->instrument = cell->instrument;
    c->note = note;
    return;
  }

  int inst = (cell->instrument == MSM_INST_NONE) ? -1 : cell->instrument;
  voice_note_on(p, ch, note, inst);
}

static void process_row(struct msm_player *p) {
  int pat = current_pattern(p);
  const msm_pattern_t *pattern = &p->song->patterns[pat];

  for (int ch = 0; ch < MSM_CHANNELS; ch++) {
    msm_voice_t *c = &p->v[ch];
    c->cell = pattern->cell[p->row][ch];   /* cópia estável p/ os ticks */
    c->delayed_note = -1;
    const msm_cell_t *cell = &c->cell;
    int fx = cell->effect, param = cell->param;

    /* Instrumento sem nota: troca e restaura volume (estilo PT) */
    if (cell->note == MSM_NOTE_NONE && cell->instrument != MSM_INST_NONE &&
        cell->instrument < p->song->header.instrument_count) {
      c->instrument = cell->instrument;
      c->volume = 64;
    }

    /* Memórias de efeito (antes do trigger, p/ 3xx pegar speed novo) */
    switch (fx) {
      case MSM_FX_TONE_PORTA:
        if (param) c->porta_speed = (uint8_t)param;
        break;
      case MSM_FX_PORTA_UP:
      case MSM_FX_PORTA_DOWN:
        if (param) c->slide_speed = (uint8_t)param;
        break;
      case MSM_FX_VOL_SLIDE:
        if (param) c->vol_slide = (uint8_t)param;
        break;
      case MSM_FX_VIBRATO:
        if (param & 0x0F) c->vib_param = (uint8_t)((c->vib_param & 0xF0) | (param & 0x0F));
        if (param & 0xF0) c->vib_param = (uint8_t)((c->vib_param & 0x0F) | (param & 0xF0));
        break;
      case MSM_FX_TREMOLO:
        if (param) c->trem_param = (uint8_t)param;
        break;
    }

    /* Nota nova com volume cheio (a não ser que Cxx mude já já) */
    if (cell->note < 120) c->volume = 64;

    /* Dispara (ou agenda, no caso de EDx) */
    if (cell->note != MSM_NOTE_NONE) {
      if (fx == MSM_FX_EXTENDED && (param >> 4) == 0xD && (param & 0x0F) > 0)
        c->delayed_note = param & 0x0F;
      else
        trigger_cell(p, ch);
    }

    /* Efeitos de tick 0 */
    switch (fx) {
      case MSM_FX_VOLUME:
        c->volume = param > 64 ? 64 : param;
        break;
      case MSM_FX_JUMP:
        p->jump_position = param;
        p->break_request = true;
        p->break_row = 0;
        break;
      case MSM_FX_BREAK: {
        int r = ((param >> 4) & 0x0F) * 10 + (param & 0x0F);
        p->break_request = true;
        p->break_row = (r < MSM_ROWS) ? r : 0;
        break;
      }
      case MSM_FX_SPEED:
        if (param == 0) break;
        if (param < 0x20) p->speed = param;
        else { p->bpm = param; update_tick_len(p); }
        break;
    }
  }
}

/* ─── Efeitos por tick (1..speed-1) ────────────────────────────────────── */

static void apply_tick_effects(struct msm_player *p, int ch) {
  msm_voice_t *c = &p->v[ch];
  const msm_cell_t *cell = &c->cell;
  int fx = cell->effect, param = cell->param;

  /* Nota atrasada (EDx) */
  if (c->delayed_note >= 0 && p->tick == c->delayed_note) {
    c->delayed_note = -1;
    trigger_cell(p, ch);
  }

  if (!c->voice) return;

  float vib_semi = 0.0f;

  switch (fx) {
    case MSM_FX_ARPEGGIO:
      if (param) {
        int semi[3] = { 0, (param >> 4) & 0x0F, param & 0x0F };
        c->period_out = c->period_base * pow2_down[semi[p->tick % 3]];
        gfxa_sfxr_set_period(c->voice, c->period_out);
        return;
      }
      break;

    case MSM_FX_PORTA_UP: {
      int sp = param ? param : c->slide_speed;
      c->period_base -= (float)sp * MSM_PORTA_SCALE;
      if (c->period_base < 16.0f) c->period_base = 16.0f;
      break;
    }

    case MSM_FX_PORTA_DOWN: {
      int sp = param ? param : c->slide_speed;
      c->period_base += (float)sp * MSM_PORTA_SCALE;
      if (c->period_base > 65535.0f) c->period_base = 65535.0f;
      break;
    }

    case MSM_FX_TONE_PORTA:
      if (c->porta_target > 0.0f && c->porta_speed) {
        float step = (float)c->porta_speed * MSM_PORTA_SCALE;
        if (c->period_base < c->porta_target) {
          c->period_base += step;
          if (c->period_base > c->porta_target) c->period_base = c->porta_target;
        } else {
          c->period_base -= step;
          if (c->period_base < c->porta_target) c->period_base = c->porta_target;
        }
      }
      break;

    case MSM_FX_VIBRATO: {
      int spd = (c->vib_param >> 4) & 0x0F;
      int dep = c->vib_param & 0x0F;
      c->vib_phase += spd;
      vib_semi = sinf((float)c->vib_phase * (2.0f * (float)M_PI / 64.0f)) *
                 ((float)dep / 15.0f);
      break;
    }

    case MSM_FX_TREMOLO: {
      int spd = (c->trem_param >> 4) & 0x0F;
      int dep = c->trem_param & 0x0F;
      c->trem_phase += spd;
      float mod = sinf((float)c->trem_phase * (2.0f * (float)M_PI / 64.0f)) *
                  ((float)dep / 15.0f) * 32.0f;
      int v = c->volume + (int)mod;
      c->volume = v < 0 ? 0 : v > 64 ? 64 : v;
      break;
    }

    case MSM_FX_VOL_SLIDE: {
      int pr = param ? param : c->vol_slide;
      int up = (pr >> 4) & 0x0F, down = pr & 0x0F;
      c->volume += up ? up : -down;
      if (c->volume > 64) c->volume = 64;
      if (c->volume < 0)  c->volume = 0;
      break;
    }

    case MSM_FX_EXTENDED:
      if ((param >> 4) == 0xC && p->tick == (param & 0x0F))
        voice_release(c, MSM_DECLICK_SAMPLES);
      break;
  }

  /* Aplica pitch (base + vibrato) */
  if (c->voice) {
    float period = c->period_base;
    if (vib_semi != 0.0f) period *= powf(2.0f, -vib_semi / 12.0f);
    if (period != c->period_out || vib_semi != 0.0f ||
        fx == MSM_FX_PORTA_UP || fx == MSM_FX_PORTA_DOWN ||
        fx == MSM_FX_TONE_PORTA) {
      c->period_out = period;
      gfxa_sfxr_set_period(c->voice, period);
    }
  }
}

/* ─── Avanço do sequenciador ───────────────────────────────────────────── */

static void advance_row(struct msm_player *p) {
  if (p->break_request) {
    p->break_request = false;
    int next_pos = (p->jump_position >= 0) ? p->jump_position : p->position + 1;
    p->jump_position = -1;
    p->row = p->break_row;
    p->break_row = 0;
    if (!p->loop_pattern) p->position = next_pos;
  } else {
    p->row++;
    if (p->row >= MSM_ROWS) {
      p->row = 0;
      if (!p->loop_pattern) p->position++;
    }
  }

  if (p->position >= p->song->header.sequence_length) {
    int restart = p->song->header.restart;
    if (restart >= p->song->header.sequence_length) restart = 0;
    p->position = restart;
    if (p->row >= MSM_ROWS) p->row = 0;
  }
}

static void sequencer_tick(struct msm_player *p) {
  p->tick++;
  if (p->tick >= p->speed) {
    p->tick = 0;
    advance_row(p);
    process_row(p);
  } else {
    for (int ch = 0; ch < MSM_CHANNELS; ch++)
      apply_tick_effects(p, ch);
  }
}

/* ─── Mixer ────────────────────────────────────────────────────────────── */

static void reset_voices(struct msm_player *p, int fade);

/* Consome notas de jam pendentes (vindas da UI). */
static void consume_jam(struct msm_player *p) {
  for (int ch = 0; ch < MSM_CHANNELS; ch++) {
    msm_voice_t *c = &p->v[ch];
    if (!c->jam_flag) continue;
    int note = c->jam_note, inst = c->jam_inst;
    c->jam_flag = 0;
    if (note == MSM_NOTE_CUT || note == MSM_NOTE_OFF) {
      voice_release(c, MSM_RELEASE_SAMPLES);
    } else if (note < 120) {
      voice_note_on(p, ch, note, inst < MSM_MAX_INSTRUMENTS ? inst : -1);
      c->volume = 64;
    }
  }
}

/* Mixa `n` samples de um canal em buf (com saturação no final do fill). */
static void mix_voice(struct msm_player *p, int ch, int32_t *acc, int n) {
  msm_voice_t *c = &p->v[ch];
  int chvol_q15 = c->muted ? 0 : (p->global_vol_q15 * c->volume) / 64;

  if (c->voice) {
    int got = gfxa_sfxr_read(c->voice, p->tmp, n);
    for (int i = 0; i < got; i++)
      acc[i] += ((int32_t)p->tmp[i] * chvol_q15) >> 15;
    if (got < n) {   /* envelope terminou */
      gfxa_sfxr_destroy(c->voice);
      c->voice = NULL;
      c->note = -1;
    }
  }

  for (int d = 0; d < MSM_DYING_VOICES; d++) {
    if (!c->dying[d]) continue;
    int want = n < c->dying_left[d] ? n : c->dying_left[d];
    int got = gfxa_sfxr_read(c->dying[d], p->tmp, want);
    for (int i = 0; i < got; i++) {
      int fade_q15 = ((c->dying_left[d] - i) * MSM_Q15) / c->dying_len[d];
      int gain_q15 = (chvol_q15 * fade_q15) >> 15;
      acc[i] += ((int32_t)p->tmp[i] * gain_q15) >> 15;
    }
    c->dying_left[d] -= got;
    if (got < want || c->dying_left[d] <= 0) {
      gfxa_sfxr_destroy(c->dying[d]);
      c->dying[d] = NULL;
      c->dying_left[d] = 0;
    }
  }
}

int msm_fill(int16_t *buf, int n, void *userdata) {
  struct msm_player *p = (struct msm_player *)userdata;
  memset(buf, 0, (size_t)n * sizeof(int16_t));
  if (!p) return n;
  if (p->edit_lock) return n;   /* UI mexendo na song: silêncio */

  /* pedidos de transporte vindos da UI */
  if (p->silence_req) {
    p->silence_req = 0;
    reset_voices(p, MSM_RELEASE_SAMPLES);
  }
  if (p->restart_req) {
    p->restart_req = 0;
    reset_voices(p, MSM_DECLICK_SAMPLES);
    p->tick = 0;
    p->break_request = false;
    p->break_row = 0;
    p->jump_position = -1;
    p->speed = p->song->header.speed > 0 ? p->song->header.speed
                                         : MSM_SPEED_DEFAULT;
    p->bpm   = p->song->header.bpm   > 0 ? p->song->header.bpm
                                         : MSM_BPM_DEFAULT;
    update_tick_len(p);
    p->samples_left = p->samples_per_tick;
    process_row(p);
  }

  consume_jam(p);

  int32_t acc[MSM_MIX_CHUNK];
  int off = 0;
  while (off < n) {
    if (p->playing && p->samples_left <= 0) {
      sequencer_tick(p);
      p->samples_left = p->samples_per_tick;
    }

    int chunk = n - off;
    if (chunk > MSM_MIX_CHUNK) chunk = MSM_MIX_CHUNK;
    if (p->playing && chunk > p->samples_left) chunk = p->samples_left;

    memset(acc, 0, (size_t)chunk * sizeof(int32_t));
    for (int ch = 0; ch < MSM_CHANNELS; ch++)
      mix_voice(p, ch, acc, chunk);

    for (int i = 0; i < chunk; i++) {
      int32_t s = acc[i];
      /* joelho suave acima de ~0.73 fs: comprime 4:1 antes de saturar */
      if (s > 24000)       s = 24000 + (s - 24000) / 4;
      else if (s < -24000) s = -24000 + (s + 24000) / 4;
      if (s > 32767) s = 32767;
      else if (s < -32768) s = -32768;
      buf[off + i] = (int16_t)s;
    }

    off += chunk;
    if (p->playing) p->samples_left -= chunk;
  }
  return n;
}

/* ─── API do player ────────────────────────────────────────────────────── */

msm_player_t *msm_create(msm_song_t *song) {
  if (!song) return NULL;
  msm_player_t *p = (msm_player_t *)calloc(1, sizeof(*p));
  if (!p) return NULL;

  p->song = song;
  p->speed = song->header.speed > 0 ? song->header.speed : MSM_SPEED_DEFAULT;
  p->bpm   = song->header.bpm   > 0 ? song->header.bpm   : MSM_BPM_DEFAULT;
  p->global_vol = 0.75f;
  p->global_vol_q15 = float_to_q15(p->global_vol);
  p->jump_position = -1;
  init_midi_freq();
  update_tick_len(p);

  for (int i = 0; i < MSM_CHANNELS; i++) {
    p->v[i].note = -1;
    p->v[i].instrument = -1;
    p->v[i].volume = 64;
    p->v[i].delayed_note = -1;
  }
  return p;
}

static void reset_voices(struct msm_player *p, int fade) {
  for (int i = 0; i < MSM_CHANNELS; i++) {
    msm_voice_t *c = &p->v[i];
    if (c->voice) voice_release(c, fade);
    c->porta_target = 0.0f;
    c->vib_phase = c->trem_phase = 0;
    c->delayed_note = -1;
    c->volume = 64;
  }
}

void msm_play_from(msm_player_t *p, int pos, int row) {
  if (!p) return;
  p->playing = 0;   /* pausa o sequenciador enquanto reposiciona */

  int len = p->song->header.sequence_length;
  if (pos < 0) pos = 0;
  if (len > 0 && pos >= len) pos = len - 1;
  if (row < 0) row = 0;
  if (row >= MSM_ROWS) row = MSM_ROWS - 1;

  p->position = pos;
  p->row = row;

  /* O reposicionamento das vozes acontece na thread de áudio (fill);
   * aqui só registramos o pedido. */
  p->silence_req = 0;
  p->restart_req = 1;
  p->playing = 1;
}

void msm_play(msm_player_t *p) { msm_play_from(p, 0, 0); }

void msm_stop(msm_player_t *p) {
  if (!p) return;
  p->playing = 0;
  p->silence_req = 1;
}

bool msm_is_playing(const msm_player_t *p) { return p && p->playing; }

void msm_set_loop_pattern(msm_player_t *p, bool loop) { if (p) p->loop_pattern = loop; }
bool msm_get_loop_pattern(const msm_player_t *p) { return p && p->loop_pattern; }

int msm_get_position(const msm_player_t *p) { return p ? p->position : 0; }
int msm_get_row(const msm_player_t *p)      { return p ? p->row : 0; }
int msm_get_tick(const msm_player_t *p)     { return p ? p->tick : 0; }
int msm_get_current_pattern(const msm_player_t *p) {
  return p ? current_pattern(p) : 0;
}

void msm_set_position(msm_player_t *p, int pos) {
  if (!p) return;
  if (p->playing) {
    msm_play_from(p, pos, 0);
  } else {
    int len = p->song->header.sequence_length;
    if (pos < 0) pos = 0;
    if (len > 0 && pos >= len) pos = len - 1;
    p->position = pos;
    p->row = 0;
    p->tick = 0;
  }
}

int  msm_get_speed(const msm_player_t *p) { return p ? p->speed : MSM_SPEED_DEFAULT; }
void msm_set_speed(msm_player_t *p, int speed) {
  if (!p) return;
  if (speed < 1) speed = 1;
  if (speed > 31) speed = 31;
  p->speed = speed;
}

int  msm_get_bpm(const msm_player_t *p) { return p ? p->bpm : MSM_BPM_DEFAULT; }
void msm_set_bpm(msm_player_t *p, int bpm) {
  if (!p) return;
  if (bpm < 32) bpm = 32;
  if (bpm > 255) bpm = 255;
  p->bpm = bpm;
  update_tick_len(p);
}

void msm_set_global_volume(msm_player_t *p, float vol) {
  if (!p) return;
  p->global_vol = vol < 0.0f ? 0.0f : vol > 1.0f ? 1.0f : vol;
  p->global_vol_q15 = float_to_q15(p->global_vol);
}
float msm_get_global_volume(const msm_player_t *p) {
  return p ? p->global_vol : 0.0f;
}

void msm_set_mute(msm_player_t *p, int ch, bool mute) {
  if (p && ch >= 0 && ch < MSM_CHANNELS) p->v[ch].muted = mute;
}
bool msm_get_mute(const msm_player_t *p, int ch) {
  return p && ch >= 0 && ch < MSM_CHANNELS && p->v[ch].muted;
}

void msm_jam(msm_player_t *p, int ch, int note, int instrument) {
  if (!p || ch < 0 || ch >= MSM_CHANNELS) return;
  msm_voice_t *c = &p->v[ch];
  c->jam_note = (uint8_t)note;
  c->jam_inst = (uint8_t)(instrument < 0 ? 0xFF : instrument);
  c->jam_flag = 1;   /* flag por último (o áudio lê depois de ver a flag) */
}

int msm_get_channel_note(const msm_player_t *p, int ch) {
  if (!p || ch < 0 || ch >= MSM_CHANNELS) return -1;
  return p->v[ch].voice ? p->v[ch].note : -1;
}

void msm_edit_lock(msm_player_t *p)   { if (p) p->edit_lock = 1; }
void msm_edit_unlock(msm_player_t *p) { if (p) p->edit_lock = 0; }

void msm_destroy(msm_player_t *p) {
  if (!p) return;
  p->playing = 0;
  for (int i = 0; i < MSM_CHANNELS; i++)
    voice_free_all(&p->v[i]);
  free(p);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Instrumentos padrão e song vazia
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct { const char *name; float p[GFXA_SFXR_PARAM_COUNT]; } def_inst_t;

/* Índices: ver sfxr.h. Campos não listados ficam no default. */
static void build_inst(msm_instrument_t *out, const char *name,
                       int wave, float atk, float sus, float punch, float dec,
                       float duty, float freq_ramp, float lpf, float hpf,
                       float vol) {
  float p[GFXA_SFXR_PARAM_COUNT];
  gfxa_sfxr_defaults(p);
  p[GFXA_SFXR_WAVE_TYPE]     = (float)wave;
  p[GFXA_SFXR_ATTACK]        = atk;
  p[GFXA_SFXR_SUSTAIN_TIME]  = sus;
  p[GFXA_SFXR_SUSTAIN_PUNCH] = punch;
  p[GFXA_SFXR_DECAY]         = dec;
  p[GFXA_SFXR_BASE_FREQ]     = 0.3f;
  p[GFXA_SFXR_FREQ_RAMP]     = freq_ramp;
  p[GFXA_SFXR_DUTY]          = duty;
  p[GFXA_SFXR_LPF_FREQ]      = lpf;
  p[GFXA_SFXR_HPF_FREQ]      = hpf;
  p[GFXA_SFXR_SOUND_VOL]     = vol * 0.3;
  gfxa_sfxr_pack(p, out->params);
  snprintf(out->name, sizeof(out->name), "%s", name);
}

void msm_default_instruments(msm_song_t *song) {
  if (!song) return;
  msm_instrument_t *ins = song->instruments;
  /* SOUND_VOL: gain = e^v - 1, e a onda quadrada tem metade da amplitude
   * de saw/sine/noise — estes valores deixam cada voz em ~0.25..0.45 do
   * fundo de escala para 4 canais somarem sem saturar.                  */
  /*                                wave atk    sus   punch dec   duty  ramp  lpf   hpf   vol  */
  build_inst(&ins[0], "SqLead",     0,   0.01f, 0.20f, 0.15f, 0.25f, 0.25f, 0.50f, 1.0f, 0.0f, 0.42f);
  build_inst(&ins[1], "SqThin",     0,   0.01f, 0.12f, 0.10f, 0.18f, 0.50f, 0.50f, 1.0f, 0.0f, 0.38f);
  build_inst(&ins[2], "SawBass",    1,   0.00f, 0.26f, 0.20f, 0.22f, 0.00f, 0.50f, 0.62f, 0.0f, 0.22f);
  build_inst(&ins[3], "SinePad",    2,   0.06f, 0.30f, 0.05f, 0.35f, 0.00f, 0.50f, 1.0f, 0.0f, 0.24f);
  build_inst(&ins[4], "Kick",       2,   0.00f, 0.10f, 0.20f, 0.20f, 0.00f, 0.22f, 1.0f, 0.0f, 0.32f);
  build_inst(&ins[5], "Snare",      3,   0.00f, 0.06f, 0.15f, 0.20f, 0.00f, 0.50f, 0.55f, 0.10f, 0.30f);
  build_inst(&ins[6], "HiHat",      3,   0.00f, 0.02f, 0.00f, 0.09f, 0.00f, 0.50f, 1.0f, 0.45f, 0.30f);
  build_inst(&ins[7], "Pluck",      0,   0.00f, 0.06f, 0.10f, 0.16f, 0.35f, 0.50f, 0.85f, 0.0f, 0.38f);
  if (song->header.instrument_count < 8)
    song->header.instrument_count = 8;
}

void msm_song_init(msm_song_t *song, const char *name) {
  if (!song) return;
  memset(song, 0, sizeof(*song));
  memcpy(song->header.magic, "MSM1", 4);
  snprintf(song->header.name, sizeof(song->header.name), "%s",
           name ? name : "untitled");
  song->header.speed = MSM_SPEED_DEFAULT;
  song->header.bpm = MSM_BPM_DEFAULT;
  song->header.pattern_count = 1;
  song->header.instrument_count = 0;
  song->header.sequence_length = 1;
  song->header.channels = MSM_CHANNELS;
  song->header.rows_per_pattern = MSM_ROWS;
  song->header.restart = 0;
  for (int r = 0; r < MSM_ROWS; r++)
    for (int ch = 0; ch < MSM_CHANNELS; ch++) {
      msm_cell_t *c = &song->patterns[0].cell[r][ch];
      c->note = MSM_NOTE_NONE;
      c->instrument = MSM_INST_NONE;
    }
  msm_default_instruments(song);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Serialização .MSM
 *  Layout: header | instruments[count] | sequence[len] | patterns[count]
 *  (structs só com uint8/char — sem padding nem endianness)
 * ══════════════════════════════════════════════════════════════════════════ */

static int header_valid(const msm_header_t *h) {
  if (memcmp(h->magic, "MSM1", 4) != 0) return 0;
  if (h->channels != MSM_CHANNELS) return 0;
  if (h->rows_per_pattern != MSM_ROWS) return 0;
  if (h->instrument_count > MSM_MAX_INSTRUMENTS) return 0;
  if (h->pattern_count == 0 || h->pattern_count > MSM_MAX_PATTERNS) return 0;
  if (h->sequence_length == 0 || h->sequence_length > MSM_MAX_SEQUENCE) return 0;
  return 1;
}

int msm_save_file(const msm_song_t *song, FILE *f) {
  if (!song || !f) return 1;
  const msm_header_t *h = &song->header;
  if (fwrite(h, sizeof(*h), 1, f) != 1) return 1;
  if (h->instrument_count &&
      fwrite(song->instruments, sizeof(msm_instrument_t),
             h->instrument_count, f) != h->instrument_count) return 1;
  if (fwrite(song->sequence, 1, h->sequence_length, f) != h->sequence_length)
    return 1;
  if (fwrite(song->patterns, sizeof(msm_pattern_t),
             h->pattern_count, f) != h->pattern_count) return 1;
  return 0;
}

int msm_load_file(msm_song_t *song, FILE *f) {
  if (!song || !f) return 1;
  memset(song, 0, sizeof(*song));
  msm_header_t *h = &song->header;
  if (fread(h, sizeof(*h), 1, f) != 1) return 1;
  if (!header_valid(h)) return 1;
  if (h->instrument_count &&
      fread(song->instruments, sizeof(msm_instrument_t),
            h->instrument_count, f) != h->instrument_count) return 1;
  if (fread(song->sequence, 1, h->sequence_length, f) != h->sequence_length)
    return 1;
  if (fread(song->patterns, sizeof(msm_pattern_t),
            h->pattern_count, f) != h->pattern_count) return 1;
  return 0;
}

int msm_pack(const msm_song_t *song, uint8_t *buf, int buf_size) {
  if (!song || !buf) return 0;
  const msm_header_t *h = &song->header;
  int inst_size = h->instrument_count * (int)sizeof(msm_instrument_t);
  int seq_size  = h->sequence_length;
  int pat_size  = h->pattern_count * (int)sizeof(msm_pattern_t);
  int total = (int)sizeof(*h) + inst_size + seq_size + pat_size;
  if (buf_size < total) return 0;

  uint8_t *ptr = buf;
  memcpy(ptr, h, sizeof(*h));                 ptr += sizeof(*h);
  memcpy(ptr, song->instruments, inst_size);  ptr += inst_size;
  memcpy(ptr, song->sequence, seq_size);      ptr += seq_size;
  memcpy(ptr, song->patterns, pat_size);
  return total;
}

int msm_unpack(const uint8_t *buf, int buf_size, msm_song_t *song) {
  if (!buf || !song || buf_size < (int)sizeof(msm_header_t)) return 1;
  memset(song, 0, sizeof(*song));
  msm_header_t *h = &song->header;
  memcpy(h, buf, sizeof(*h));
  if (!header_valid(h)) return 1;

  int inst_size = h->instrument_count * (int)sizeof(msm_instrument_t);
  int seq_size  = h->sequence_length;
  int pat_size  = h->pattern_count * (int)sizeof(msm_pattern_t);
  int total = (int)sizeof(*h) + inst_size + seq_size + pat_size;
  if (buf_size < total) return 1;

  const uint8_t *ptr = buf + sizeof(*h);
  memcpy(song->instruments, ptr, inst_size);  ptr += inst_size;
  memcpy(song->sequence, ptr, seq_size);      ptr += seq_size;
  memcpy(song->patterns, ptr, pat_size);
  return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Importador ProTracker .MOD (4 canais, "M.K." e variantes)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Tabela de períodos Amiga (finetune 0): C-1..B-3 do ProTracker */
static const uint16_t pt_periods[36] = {
  856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
  428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
  214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

/* Período Amiga → nota MIDI. PT C-1 (856) é mapeado para MIDI 48 (C3),
 * o registro médio do tracker. Aproxima para a nota mais próxima. */
static int pt_period_to_midi(int period) {
  if (period <= 0) return -1;
  int best = 0, best_d = 1 << 30;
  for (int i = 0; i < 36; i++) {
    int d = period - pt_periods[i];
    if (d < 0) d = -d;
    if (d < best_d) { best_d = d; best = i; }
  }
  return 48 + best;
}

int msm_import_mod(const uint8_t *buf, int size, msm_song_t *song) {
  /* Header MOD: title[20] + 31*30 (samples) + len + restart + order[128] + magic */
  if (!buf || !song || size < 1084) return 1;

  const uint8_t *magic = buf + 1080;
  if (!(memcmp(magic, "M.K.", 4) == 0 || memcmp(magic, "M!K!", 4) == 0 ||
        memcmp(magic, "FLT4", 4) == 0 || memcmp(magic, "4CHN", 4) == 0))
    return 1;   /* só MODs de 4 canais */

  int song_len = buf[950];
  int restart  = buf[951];
  if (song_len < 1 || song_len > 128) return 1;

  int max_pat = 0;
  for (int i = 0; i < 128; i++)
    if (buf[952 + i] > max_pat) max_pat = buf[952 + i];
  int pat_count = max_pat + 1;
  if (pat_count > MSM_MAX_PATTERNS) return 1;
  if (size < 1084 + pat_count * 1024) return 1;

  msm_song_init(song, "");
  memcpy(song->header.name, buf, 20);
  song->header.name[20] = '\0';
  if (!song->header.name[0])
    snprintf(song->header.name, sizeof(song->header.name), "mod import");

  song->header.pattern_count   = (uint8_t)pat_count;
  song->header.sequence_length = (uint8_t)(song_len > MSM_MAX_SEQUENCE
                                           ? MSM_MAX_SEQUENCE : song_len);
  song->header.restart = (uint8_t)(restart < song_len ? restart : 0);
  memcpy(song->sequence, buf + 952, song->header.sequence_length);

  /* Samples → instrumentos: roda o banco padrão e copia nomes/volume.
   * Sample n (1..31) vira instrumento (n-1) % 32. */
  int used_inst = 0;
  for (int i = 0; i < 31 && i < MSM_MAX_INSTRUMENTS; i++) {
    const uint8_t *smp = buf + 20 + i * 30;
    int length = (smp[22] << 8) | smp[23];
    if (length <= 1) continue;
    msm_instrument_t *dst = &song->instruments[i];
    if (i >= 8)  /* fora do banco padrão: cicla os 8 timbres */
      *dst = song->instruments[i % 8];
    memcpy(dst->name, smp, sizeof(dst->name) - 1);
    dst->name[sizeof(dst->name) - 1] = '\0';
    if (i + 1 > used_inst) used_inst = i + 1;
  }
  if (used_inst > song->header.instrument_count)
    song->header.instrument_count = (uint8_t)used_inst;

  /* Patterns: 64 rows × 4 canais × 4 bytes */
  for (int pat = 0; pat < pat_count; pat++) {
    const uint8_t *src = buf + 1084 + pat * 1024;
    for (int r = 0; r < 64; r++) {
      for (int ch = 0; ch < 4; ch++) {
        const uint8_t *b = src + (r * 4 + ch) * 4;
        int period = ((b[0] & 0x0F) << 8) | b[1];
        int sample = (b[0] & 0xF0) | (b[2] >> 4);
        int fx     = b[2] & 0x0F;
        int param  = b[3];

        msm_cell_t *cell = &song->patterns[pat].cell[r][ch];
        int midi = pt_period_to_midi(period);
        cell->note = (midi >= 0) ? (uint8_t)midi : MSM_NOTE_NONE;
        cell->instrument = sample > 0 ? (uint8_t)((sample - 1) % 32)
                                      : MSM_INST_NONE;

        /* Efeitos: subconjunto compatível; o resto é descartado */
        switch (fx) {
          case 0x0: case 0x1: case 0x2: case 0x3: case 0x4:
          case 0x7: case 0xA: case 0xB: case 0xC: case 0xD: case 0xF:
            cell->effect = (uint8_t)fx;
            cell->param  = (uint8_t)param;
            break;
          case 0x5:  /* porta + volslide → mantém o volslide */
          case 0x6:  /* vibrato + volslide → idem */
            cell->effect = MSM_FX_VOL_SLIDE;
            cell->param  = (uint8_t)param;
            break;
          case 0xE:  /* só EC/ED têm equivalente */
            if ((param >> 4) == 0xC || (param >> 4) == 0xD) {
              cell->effect = MSM_FX_EXTENDED;
              cell->param  = (uint8_t)param;
            } else {
              cell->effect = 0; cell->param = 0;
            }
            break;
          default:
            cell->effect = 0; cell->param = 0;
            break;
        }
      }
    }
  }
  return 0;
}
