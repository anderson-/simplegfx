#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sfxr.h"
#include "audio_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  modtracker — player de módulo tracker sem samples (micro-synth module)
 *
 *  Formato .MSM (Module Synth Mini):
 *    - 4 canais
 *    - Padrão Protracker (64 rows, patterns, sequence)
 *    - Instrumentos = parâmetros sfxr compactados (24 bytes cada)
 *    - Efeitos: arpejo, portamento, vibrato, volume, pan, salto etc.
 *
 *  Uso típico:
 *    msm_player_t *p = msm_create(&minha_musica);
 *    msm_play(p);
 *    while (msm_is_playing(p)) {
 *      msm_tick(p);           // ~50 Hz
 *      gfx_delay(20);
 *    }
 *    msm_destroy(p);
 * ══════════════════════════════════════════════════════════════════════════ */

/* ─── Constantes ───────────────────────────────────────────────────────── */

#define MSM_CHANNELS           4
#define MSM_MAX_INSTRUMENTS   32
#define MSM_MAX_PATTERNS      64
#define MSM_MAX_SEQUENCE     256
#define MSM_ROWS              64
#define MSM_SPEED_DEFAULT      6
#define MSM_BPM_DEFAULT      125

#define MSM_NOTE_NONE  0xFF    /* nenhuma nota na célula */
#define MSM_NOTE_CUT   0xFE    /* nota cortada (fade-out) */
#define MSM_NOTE_OFF   0xFD    /* nota desligada (libera) */

/* ─── Estruturas de dados (formato .MSM) ──────────────────────────────── */

/* Uma célula num pattern — 4 bytes */
typedef struct {
  uint8_t note;        /* 0..119 = MIDI, MSM_NOTE_* para especiais */
  uint8_t instrument;  /* 0..31, 0xFF = sem mudança */
  uint8_t effect;      /* 0..15 (tabela Protracker) */
  uint8_t param;       /* parâmetro do efeito */
} msm_cell_t;

/* Pattern: 64 rows × 4 canais */
typedef struct {
  msm_cell_t cell[MSM_ROWS][MSM_CHANNELS];
} msm_pattern_t;

/* Instrumento: parâmetros sfxr compactados + nome */
typedef struct {
  uint8_t params[24];  /* gfxa_sfxr_pack() */
  char    name[12];
} msm_instrument_t;

/* Cabeçalho do .MSM (formato binário em disco) */
typedef struct {
  char     magic[4];           /* "MSM1" */
  char     name[22];           /* nome da música, zero-padded */
  uint8_t  speed;              /* ticks por row (default) */
  uint8_t  pattern_count;
  uint8_t  instrument_count;
  uint8_t  sequence_length;
  uint8_t  channels;           /* 4 */
  uint8_t  rows_per_pattern;   /* 64 */
  uint8_t  reserved[8];
} msm_header_t;

/* Música completa em memória */
typedef struct {
  msm_header_t      header;
  uint8_t           sequence[MSM_MAX_SEQUENCE];
  msm_instrument_t  instruments[MSM_MAX_INSTRUMENTS];
  msm_pattern_t     patterns[MSM_MAX_PATTERNS];
} msm_song_t;

/* ─── Player (estado opaco) ────────────────────────────────────────────── */

typedef struct msm_player msm_player_t;

/* Cria um player para a música. Não copia a song — a song precisa
 * permanecer válida enquanto o player existir. */
msm_player_t *msm_create(const msm_song_t *song);

/* Inicia/para reprodução */
void msm_play(msm_player_t *p);
void msm_stop(msm_player_t *p);

/* Tick do sequenciador. Chame periodicamente (~50 Hz, ou dentro
 * do loop principal com gfx_delay). */
void msm_tick(msm_player_t *p);

/* Controles de playback */
void msm_set_position(msm_player_t *p, int pos);
void msm_set_row(msm_player_t *p, int row);

/* Estado */
int  msm_get_position(const msm_player_t *p);
int  msm_get_row(const msm_player_t *p);
int  msm_get_tick(const msm_player_t *p);
int  msm_get_speed(const msm_player_t *p);
void msm_set_speed(msm_player_t *p, int speed);
int  msm_get_sequence_length(const msm_player_t *p);
int  msm_get_current_pattern(const msm_player_t *p);
bool msm_is_playing(const msm_player_t *p);

/* Ajuste de volume global (0.0..1.0) e por canal */
void msm_set_global_volume(msm_player_t *p, float vol);
float msm_get_global_volume(const msm_player_t *p);

/* Destrói o player e libera recursos (para sons em execução) */
void msm_destroy(msm_player_t *p);

/* ─── Helpers de serialização ─────────────────────────────────────────── */

/* Empacota música para buffer binário .MSM. Retorna bytes escritos. */
int msm_pack(const msm_song_t *song, uint8_t *buf, int buf_size);

/* Desempacota buffer .MSM para memória. Retorna 0 em sucesso. */
int msm_unpack(const uint8_t *buf, int buf_size, msm_song_t *song);

/* Converte nota MIDI → frequência Hz */
float msm_midi_to_freq(int midi_note);

#ifdef __cplusplus
}
#endif
