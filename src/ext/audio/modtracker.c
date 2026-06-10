#include "modtracker.h"
#include "simplegfx.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  Internal channel state
 * ══════════════════════════════════════════════════════════════════════════ */

#define FX_ARPEGGIO         0x0
#define FX_PORTAMENTO_UP    0x1
#define FX_PORTAMENTO_DOWN  0x2
#define FX_TONE_PORTAMENTO  0x3
#define FX_VIBRATO          0x4
#define FX_TREMOLO          0x7
#define FX_PANNING          0x8
#define FX_VOLUME_SLIDE     0xA
#define FX_POSITION_JUMP    0xB
#define FX_SET_VOLUME       0xC
#define FX_PATTERN_BREAK    0xD
#define FX_EXTENDED         0xE
#define FX_SET_SPEED        0xF

typedef struct {
  /* Notas */
  int   note;             /* MIDI atual (-1 = nenhum) */
  int   instrument;       /* índice do instrumento */
  int   volume;           /* volume 0..64 */

  /* Engine */
  int   engine_ch;        /* canal no gfxa_engine (-1 = inativo) */
  struct sfxr_state *sfxr;/* instância sfxr atual */

  /* Efeitos persistentes */
  struct {
    int arp_note[2];      /* semitons de offset (0xy) */
    int arp_phase;        /* 0, 1, 2 (ciclo de 3 ticks) */
    int porta_speed;      /* velocidade do portamento */
    int porta_target;     /* MIDI alvo (-1 = nenhum) */
    int vib_speed;        /* velocity vibrato (0..15) */
    int vib_depth;        /* depth vibrato (0..15) */
    int vib_phase;        /* fase acumulada */
    int trem_speed;
    int trem_depth;
    int trem_phase;
    int vol_slide_up;
    int vol_slide_down;
  } fx;
} msm_ch_state_t;

struct msm_player {
  const msm_song_t *song;
  int   playing;
  int   speed;             /* ticks por row */
  int   position;          /* posição na sequência */
  int   row;               /* row atual no pattern */
  int   tick;              /* tick atual dentro do row */
  float global_vol;        /* 0.0..1.0 */
  bool  break_request;     /* pattern break pendente */
  int   break_row;         /* row para onde ir no break */
  int   jump_position;     /* position jump */

  msm_ch_state_t ch[MSM_CHANNELS];
};

/* ══════════════════════════════════════════════════════════════════════════
 *  Helpers
 * ══════════════════════════════════════════════════════════════════════════ */

float msm_midi_to_freq(int midi_note) {
  return 440.0f * powf(2.0f, (float)(midi_note - 69) / 12.0f);
}

/* Busca o pattern atual baseado na sequência */
static int current_pattern(const struct msm_player *p) {
  if (!p->song || p->position >= p->song->header.sequence_length)
    return 0;
  return p->song->sequence[p->position];
}

/* Retorna célula atual */
static const msm_cell_t *current_cell(const struct msm_player *p, int ch) {
  int pat = current_pattern(p);
  if (pat < 0 || pat >= p->song->header.pattern_count) return NULL;
  return &p->song->patterns[pat].cell[p->row][ch];
}

/* Para um canal: só sinaliza o engine. channel_sync() detecta
 * quando o canal realmente terminou. */
static void channel_stop(struct msm_player *p, int ch) {
  msm_ch_state_t *c = &p->ch[ch];
  if (c->engine_ch >= 0) {
    gfxa_engine_stop(c->engine_ch);
  }
}

/* Contexto usado como userdata do engine. O dtor só libera memória,
 * NUNCA mexe nos ponteiros do player (evita race conditions). */
typedef struct {
  struct sfxr_state *sfxr;
} msm_chan_ctx_t;

static void msm_chan_dtor(void *user) {
  msm_chan_ctx_t *ctx = (msm_chan_ctx_t *)user;
  if (!ctx) return;
  if (ctx->sfxr) gfxa_sfxr_destroy(ctx->sfxr);
  free(ctx);
}

/* Wrapper: gfxa_sfxr_read tem ordem (state, buf, n) mas audio_fill_fn
 * espera (buf, n, userdata). Extrai o sfxr_state do ctx. */
static int msm_fill_fn(int16_t *buf, int n, void *userdata) {
  msm_chan_ctx_t *ctx = (msm_chan_ctx_t *)userdata;
  if (!ctx || !ctx->sfxr) return 0;
  return gfxa_sfxr_read(ctx->sfxr, buf, n);
}

/* Verifica se o canal do engine ainda está ativo. Se não estiver,
 * limpa os ponteiros locais (detecta que o engine já deu free). */
static void channel_sync(msm_player_t *p, int ch) {
  msm_ch_state_t *c = &p->ch[ch];
  if (c->engine_ch < 0) return;
  if (!gfxa_engine_is_channel_active(c->engine_ch)) {
    c->sfxr = NULL;
    c->engine_ch = -1;
    c->note = -1;
  }
}

/* Dispara nota num canal */
static void channel_note_on(struct msm_player *p, int ch, int midi_note,
                            int instrument, const msm_cell_t *cell) {
  msm_ch_state_t *c = &p->ch[ch];
  const msm_song_t *song = p->song;

  /* Para o canal primeiro (fade-out + dtor) */
  if (c->engine_ch >= 0) {
    gfxa_engine_stop(c->engine_ch);
    /* Aguarda um pouco para o fade acontecer antes de tocar de novo.
     * O play_on já faz isso internamente. */
  }

  /* Se instrumento válido, troca */
  if (instrument >= 0 && instrument < song->header.instrument_count)
    c->instrument = instrument;

  if (c->instrument < 0 || c->instrument >= song->header.instrument_count)
    return; /* sem instrumento definido */

  /* Cria state sfxr com parâmetros do instrumento */
  float params[GFXA_SFXR_PARAM_COUNT];
  gfxa_sfxr_unpack(song->instruments[c->instrument].params, params);
  struct sfxr_state *s = gfxa_sfxr_create(params);
  if (!s) return;

  /* Seta a frequência da nota (ignora BASE_FREQ do instrumento) */
  float freq = msm_midi_to_freq(midi_note);
  gfxa_sfxr_set_freq(s, freq);

  c->sfxr = s;
  c->note = midi_note;
  c->volume = 64; /* volume padrão */

  /* Cria contexto para o engine (userdata = ctx, dtor só libera) */
  msm_chan_ctx_t *ctx = (msm_chan_ctx_t *)malloc(sizeof(*ctx));
  if (!ctx) { gfxa_sfxr_destroy(s); return; }
  ctx->sfxr   = s;

  c->engine_ch = gfxa_engine_play_on(ch, msm_fill_fn, ctx, msm_chan_dtor);

  /* Aplica volume inicial */
  gfxa_engine_set_volume(c->engine_ch, p->global_vol);

  /* Se há efeito na célula, processa */
  if (cell) {
    int fx = cell->effect;
    int param = cell->param;

    /* Tone portamento (3xx) — reusa nota anterior como target */
    if (fx == FX_TONE_PORTAMENTO && param > 0) {
      c->fx.porta_target = midi_note;
      c->fx.porta_speed = param;
      /* restaura nota anterior para slide */
      /* Nota: isso é tratado no tick 0 */
    }

    /* Vibrato (4xy) */
    if (fx == FX_VIBRATO) {
      c->fx.vib_speed = (param >> 4) & 0x0F;
      c->fx.vib_depth = param & 0x0F;
      gfxa_sfxr_set_vibrato(s,
        (float)c->fx.vib_speed / 15.0f,
        (float)c->fx.vib_depth / 15.0f);
    }

    /* Set volume (Cxx) */
    if (fx == FX_SET_VOLUME && param <= 64) {
      c->volume = param;
      gfxa_engine_set_volume(c->engine_ch,
        (float)param / 64.0f * p->global_vol);
    }

    /* Panning (8xx) */
    if (fx == FX_PANNING) {
      float pan = (float)(param & 0x0F) / 7.0f - 1.0f;
      if (pan < -1.0f) pan = -1.0f;
      gfxa_engine_set_pan(c->engine_ch, pan);
    }
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Effects — aplicados a cada tick (excepto tick 0)
 * ══════════════════════════════════════════════════════════════════════════ */

static void apply_effects(struct msm_player *p, int ch,
                           const msm_cell_t *cell) {
  msm_ch_state_t *c = &p->ch[ch];
  channel_sync(p, ch);
  if (!cell) return;
  int fx = cell->effect;
  int param = cell->param;

  /* ─── Arpeggio (0xy) ───────────────────────────────── */
  if (fx == FX_ARPEGGIO && param > 0 && c->sfxr && c->note >= 0) {
    int semi[3] = {0, (param >> 4) & 0x0F, param & 0x0F};
    int phase = c->fx.arp_phase % 3;
    int note = c->note + semi[phase];
    float freq = msm_midi_to_freq(note);
    float period = 16000.0f / freq;
    if (period < 8.0f) period = 8.0f;
    gfxa_sfxr_set_period(c->sfxr, period);
    c->fx.arp_phase = (c->fx.arp_phase + 1) % 3;
  }

  /* ─── Portamento up (1xx) ──────────────────────────── */
  if (fx == FX_PORTAMENTO_UP && param > 0 && c->sfxr) {
    float period = gfxa_sfxr_get_period(c->sfxr);
    period -= (float)param;
    if (period < 8.0f) period = 8.0f;
    gfxa_sfxr_set_period(c->sfxr, period);
  }

  /* ─── Portamento down (2xx) ────────────────────────── */
  if (fx == FX_PORTAMENTO_DOWN && param > 0 && c->sfxr) {
    float period = gfxa_sfxr_get_period(c->sfxr);
    period += (float)param;
    gfxa_sfxr_set_period(c->sfxr, period);
  }

  /* ─── Tone portamento (3xx) ────────────────────────── */
  if (fx == FX_TONE_PORTAMENTO && c->fx.porta_target >= 0 && c->sfxr) {
    float target_period = 16000.0f / msm_midi_to_freq(c->fx.porta_target);
    float cur = gfxa_sfxr_get_period(c->sfxr);
    int speed = param > 0 ? param : c->fx.porta_speed;
    if (speed > 0) {
      if (cur < target_period) {
        cur += (float)speed;
        if (cur > target_period) cur = target_period;
      } else if (cur > target_period) {
        cur -= (float)speed;
        if (cur < target_period) cur = target_period;
      }
      gfxa_sfxr_set_period(c->sfxr, cur);
    }
  }

  /* ─── Vibrato (4xy) — já configurado no sfxr internamente,
     mas atualiza phase tracking pra debug ──────────────── */
  if (fx == FX_VIBRATO) {
    c->fx.vib_phase += c->fx.vib_speed;
  }

  /* ─── Tremolo (7xy) ────────────────────────────────── */
  if (fx == FX_TREMOLO && c->engine_ch >= 0) {
    int speed = (param >> 4) & 0x0F;
    int depth = param & 0x0F;
    if (speed > 0) {
      c->fx.trem_speed = speed;
      c->fx.trem_depth = depth;
    }
    float val = sinf((float)c->fx.trem_phase * (float)M_PI / 128.0f);
    float vol_mod = (float)c->volume / 64.0f * (1.0f + val * (float)depth / 64.0f);
    if (vol_mod < 0.0f) vol_mod = 0.0f;
    if (vol_mod > 1.0f) vol_mod = 1.0f;
    gfxa_engine_set_volume(c->engine_ch, vol_mod * p->global_vol);
    c->fx.trem_phase = (c->fx.trem_phase + c->fx.trem_speed) & 0xFF;
  }

  /* ─── Volume slide (Axy) ───────────────────────────── */
  if (fx == FX_VOLUME_SLIDE) {
    int up = (param >> 4) & 0x0F;
    int down = param & 0x0F;
    if (up > 0) c->fx.vol_slide_up = up;
    if (down > 0) c->fx.vol_slide_down = down;
    if (c->fx.vol_slide_up > 0) {
      c->volume += c->fx.vol_slide_up;
      if (c->volume > 64) c->volume = 64;
    } else if (c->fx.vol_slide_down > 0) {
      c->volume -= c->fx.vol_slide_down;
      if (c->volume < 0) c->volume = 0;
    }
    if (c->engine_ch >= 0)
      gfxa_engine_set_volume(c->engine_ch,
        (float)c->volume / 64.0f * p->global_vol);
  }

  /* ─── Set volume (Cxx) — também aplicado no note-on,
     mas re-aplica aqui pra consistência ───────────────── */
  if (fx == FX_SET_VOLUME && param <= 64) {
    c->volume = param;
    if (c->engine_ch >= 0)
      gfxa_engine_set_volume(c->engine_ch,
        (float)param / 64.0f * p->global_vol);
  }

  /* ─── Panning (8xx) ────────────────────────────────── */
  if (fx == FX_PANNING && c->engine_ch >= 0) {
    float pan = (float)(param & 0x0F) / 7.0f - 1.0f;
    if (pan < -1.0f) pan = -1.0f;
    gfxa_engine_set_pan(c->engine_ch, pan);
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Row processing (tick 0)
 * ══════════════════════════════════════════════════════════════════════════ */

static void process_row(struct msm_player *p) {
  for (int ch = 0; ch < MSM_CHANNELS; ch++) {
    channel_sync(p, ch);
    const msm_cell_t *cell = current_cell(p, ch);
    if (!cell) continue;

    int note = cell->note;
    int inst = cell->instrument;

    /* Note off / cut */
    if (note == MSM_NOTE_CUT) {
      channel_stop(p, ch);
      continue;
    }

    if (note == MSM_NOTE_OFF) {
      /* Para com fade-out */
      if (p->ch[ch].engine_ch >= 0)
        gfxa_engine_stop(p->ch[ch].engine_ch);
      p->ch[ch].note = -1;
      continue;
    }

    /* Se tem nota (ou tone portamento sem nota nova) */
    if (note != MSM_NOTE_NONE) {
      /* Se é tone portamento (3xx) e canal já ativo, só atualiza target */
      int fx = cell->effect;
      int param = cell->param;
      if (fx == FX_TONE_PORTAMENTO && p->ch[ch].engine_ch >= 0 &&
          p->ch[ch].note >= 0) {
        /* Mantém nota atual, só muda target */
        p->ch[ch].fx.porta_target = note;
        p->ch[ch].fx.porta_speed = param;
        /* Troca instrumento se veio novo */
        if (inst != 0xFF && inst < p->song->header.instrument_count)
          p->ch[ch].instrument = inst;
      } else {
        /* Nota normal */
        channel_note_on(p, ch, note, inst, cell);
      }
    } else {
      /* Sem nota nova — verifica se instrumento mudou mesmo sem nota */
      if (inst != 0xFF && inst < p->song->header.instrument_count)
        p->ch[ch].instrument = inst;

      /* Efeitos que funcionam sem nota (volume, pan, etc.) */
      int fx = cell->effect;
      int param = cell->param;

      switch (fx) {
        case FX_SET_VOLUME:
          if (param <= 64) {
            p->ch[ch].volume = param;
            if (p->ch[ch].engine_ch >= 0)
              gfxa_engine_set_volume(p->ch[ch].engine_ch,
                (float)param / 64.0f * p->global_vol);
          }
          break;

        case FX_PANNING:
          if (p->ch[ch].engine_ch >= 0) {
            float pan = (float)(param & 0x0F) / 7.0f - 1.0f;
            if (pan < -1.0f) pan = -1.0f;
            gfxa_engine_set_pan(p->ch[ch].engine_ch, pan);
          }
          break;

        case FX_VOLUME_SLIDE:
          if ((param >> 4) > 0) p->ch[ch].fx.vol_slide_up = (param >> 4);
          if ((param & 0x0F) > 0) p->ch[ch].fx.vol_slide_down = (param & 0x0F);
          break;
      }
    }

    /* Reseta arp phase no início de cada row (tick 0) */
    p->ch[ch].fx.arp_phase = 0;
  }

  /* ─── Efeitos de fluxo (Bxx, Dxx) ─────────────────── */
  /* Estes são processados apenas no último canal (ou globalmente) */
  for (int ch = 0; ch < MSM_CHANNELS; ch++) {
    const msm_cell_t *cell = current_cell(p, ch);
    if (!cell) continue;

    switch (cell->effect) {
      case FX_POSITION_JUMP:   /* Bxx */
        p->jump_position = cell->param;
        p->break_request = true;
        p->break_row = 0;
        break;

      case FX_PATTERN_BREAK: { /* Dxx */
        int next_row = ((cell->param >> 4) & 0x0F) * 10 +
                        (cell->param & 0x0F);
        if (next_row >= MSM_ROWS) next_row = 0;
        p->break_request = true;
        p->break_row = next_row;
        break;
      }

      case FX_SET_SPEED: {     /* Fxx */
        int new_speed = cell->param;
        if (new_speed > 0) p->speed = new_speed;
        break;
      }

      default:
        break;
    }
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Player API
 * ══════════════════════════════════════════════════════════════════════════ */

msm_player_t *msm_create(const msm_song_t *song) {
  if (!song) return NULL;
  msm_player_t *p = (msm_player_t *)calloc(1, sizeof(*p));
  if (!p) return NULL;

  p->song = song;
  p->speed = song->header.speed > 0 ? song->header.speed : MSM_SPEED_DEFAULT;
  p->global_vol = 1.0f;
  p->playing = 0;

  for (int i = 0; i < MSM_CHANNELS; i++) {
    p->ch[i].engine_ch = -1;
    p->ch[i].note = -1;
    p->ch[i].instrument = -1;
    p->ch[i].sfxr = NULL;
  }

  return p;
}

void msm_play(msm_player_t *p) {
  if (!p) return;
  p->playing = 1;
  p->position = 0;
  p->row = 0;
  p->tick = 0;
  p->break_request = false;
  p->break_row = 0;
  p->jump_position = -1;

  for (int i = 0; i < MSM_CHANNELS; i++) {
    p->ch[i].fx.arp_phase = 0;
    p->ch[i].fx.porta_target = -1;
    p->ch[i].fx.porta_speed = 0;
    p->ch[i].fx.vib_speed = 0;
    p->ch[i].fx.vib_depth = 0;
    p->ch[i].fx.vib_phase = 0;
    p->ch[i].fx.trem_speed = 0;
    p->ch[i].fx.trem_depth = 0;
    p->ch[i].fx.trem_phase = 0;
    p->ch[i].fx.vol_slide_up = 0;
    p->ch[i].fx.vol_slide_down = 0;
  }

  /* Processa primeira linha */
  process_row(p);
}

void msm_stop(msm_player_t *p) {
  if (!p) return;
  p->playing = 0;
  for (int i = 0; i < MSM_CHANNELS; i++) {
    channel_stop(p, i);
  }
}

void msm_tick(msm_player_t *p) {
  if (!p || !p->playing) return;

  /* Sincroniza estado com o engine (detecta canais que terminaram) */
  for (int ch = 0; ch < MSM_CHANNELS; ch++)
    channel_sync(p, ch);

  p->tick++;

  if (p->tick < p->speed) {
    /* Ticks de efeito (1..speed-1) */
    for (int ch = 0; ch < MSM_CHANNELS; ch++) {
      const msm_cell_t *cell = current_cell(p, ch);
      apply_effects(p, ch, cell);

      /* Volume slide contínuo mesmo sem célula Axy ativa no tick */
      msm_ch_state_t *c = &p->ch[ch];
      if (c->fx.vol_slide_up > 0 || c->fx.vol_slide_down > 0) {
        if (c->fx.vol_slide_up > 0) {
          c->volume += c->fx.vol_slide_up;
          if (c->volume > 64) c->volume = 64;
        } else {
          c->volume -= c->fx.vol_slide_down;
          if (c->volume < 0) c->volume = 0;
        }
        if (c->engine_ch >= 0)
          gfxa_engine_set_volume(c->engine_ch,
            (float)c->volume / 64.0f * p->global_vol);
      }
    }
    return;
  }

  /* ─── Tick 0 do próximo row ────────────────────────── */
  p->tick = 0;

  /* Processa break request (pattern break / position jump) */
  if (p->break_request) {
    p->break_request = false;
    if (p->jump_position >= 0) {
      p->position = p->jump_position;
      p->jump_position = -1;
    } else {
      p->position++;
    }
    p->row = p->break_row;
    p->break_row = 0;
  } else {
    p->row++;
  }

  /* Avança pattern se necessário */
  if (p->row >= MSM_ROWS) {
    p->row = 0;
    p->position++;
  }

  /* Fim da música? (sequência acabou) */
  if (p->position >= p->song->header.sequence_length) {
    msm_stop(p);
    return;
  }

  /* Processa novo row */
  process_row(p);
}

void msm_set_position(msm_player_t *p, int pos) {
  if (!p) return;
  if (pos < 0) pos = 0;
  if (pos >= p->song->header.sequence_length)
    pos = p->song->header.sequence_length - 1;
  p->position = pos;
}

void msm_set_row(msm_player_t *p, int row) {
  if (!p) return;
  if (row < 0) row = 0;
  if (row >= MSM_ROWS) row = MSM_ROWS - 1;
  p->row = row;
}

int msm_get_position(const msm_player_t *p) {
  return p ? p->position : 0;
}

int msm_get_row(const msm_player_t *p) {
  return p ? p->row : 0;
}

int msm_get_tick(const msm_player_t *p) {
  return p ? p->tick : 0;
}

int msm_get_speed(const msm_player_t *p) {
  return p ? p->speed : MSM_SPEED_DEFAULT;
}

void msm_set_speed(msm_player_t *p, int speed) {
  if (!p) return;
  if (speed < 1) speed = 1;
  p->speed = speed;
}

int msm_get_sequence_length(const msm_player_t *p) {
  return p ? (int)p->song->header.sequence_length : 0;
}

int msm_get_current_pattern(const msm_player_t *p) {
  return current_pattern(p);
}

bool msm_is_playing(const msm_player_t *p) {
  return p ? (bool)p->playing : false;
}

void msm_set_global_volume(msm_player_t *p, float vol) {
  if (!p) return;
  if (vol < 0.0f) vol = 0.0f;
  if (vol > 1.0f) vol = 1.0f;
  p->global_vol = vol;
  for (int i = 0; i < MSM_CHANNELS; i++) {
    if (p->ch[i].engine_ch >= 0)
      gfxa_engine_set_volume(p->ch[i].engine_ch,
        (float)p->ch[i].volume / 64.0f * vol);
  }
}

float msm_get_global_volume(const msm_player_t *p) {
  return p ? p->global_vol : 0.0f;
}

void msm_destroy(msm_player_t *p) {
  if (!p) return;
  msm_stop(p);
  free(p);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Serialização .MSM
 * ══════════════════════════════════════════════════════════════════════════ */

int msm_pack(const msm_song_t *song, uint8_t *buf, int buf_size) {
  if (!song || !buf) return 0;
  int hdr_size = sizeof(msm_header_t);
  int inst_size = song->header.instrument_count * sizeof(msm_instrument_t);
  int seq_size = song->header.sequence_length;
  int pat_size = song->header.pattern_count * sizeof(msm_pattern_t);
  int total = hdr_size + inst_size + seq_size + pat_size;

  if (buf_size < total) return 0;

  uint8_t *ptr = buf;
  memcpy(ptr, &song->header, hdr_size);
  ptr += hdr_size;

  memcpy(ptr, song->instruments, inst_size);
  ptr += inst_size;

  memcpy(ptr, song->sequence, seq_size);
  ptr += seq_size;

  memcpy(ptr, song->patterns, pat_size);
  return total;
}

int msm_unpack(const uint8_t *buf, int buf_size, msm_song_t *song) {
  if (!buf || !song || buf_size < (int)sizeof(msm_header_t)) return 1;

  const uint8_t *ptr = buf;
  memcpy(&song->header, ptr, sizeof(msm_header_t));
  ptr += sizeof(msm_header_t);

  /* Valida magic */
  if (memcmp(song->header.magic, "MSM1", 4) != 0) return 1;
  if (song->header.channels != 4) return 1;
  if (song->header.rows_per_pattern != MSM_ROWS) return 1;

  int ic = song->header.instrument_count;
  if (ic < 0 || ic > MSM_MAX_INSTRUMENTS) return 1;
  int pc = song->header.pattern_count;
  if (pc < 0 || pc > MSM_MAX_PATTERNS) return 1;
  int sl = song->header.sequence_length;
  if (sl < 0 || sl > MSM_MAX_SEQUENCE) return 1;

  int inst_size = ic * sizeof(msm_instrument_t);
  int seq_size = sl;
  int pat_size = pc * sizeof(msm_pattern_t);
  int total = sizeof(msm_header_t) + inst_size + seq_size + pat_size;

  if (buf_size < total) return 1;

  memset(song->instruments, 0, sizeof(song->instruments));
  memcpy(song->instruments, ptr, inst_size);
  ptr += inst_size;

  memset(song->sequence, 0, sizeof(song->sequence));
  memcpy(song->sequence, ptr, seq_size);
  ptr += seq_size;

  memset(song->patterns, 0, sizeof(song->patterns));
  memcpy(song->patterns, ptr, pat_size);

  return 0;
}
