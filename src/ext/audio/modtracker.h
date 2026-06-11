#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "sfxr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  modtracker — tracker de 4 canais sem samples (instrumentos sfxr)
 *
 *  Formato .MSM (Module Synth Mini):
 *    - 4 canais, patterns de 64 rows (estilo ProTracker)
 *    - Instrumentos = parâmetros sfxr compactados (24 bytes cada)
 *    - Efeitos clássicos: arpejo, portamento, vibrato, tremolo,
 *      volume, slides, saltos, speed/BPM
 *
 *  O player é um audio_fill_fn: o sequenciador avança DENTRO do callback
 *  de áudio, com precisão de sample (independente do FPS da UI).
 *
 *  Uso típico:
 *    msm_player_t *p = msm_create(&song);
 *    gfxa_stream(msm_fill, p, NULL, GFXA_SAMPLE_RATE, GFXA_ASYNC);
 *    msm_play(p);                 // começa a tocar
 *    ...                          // UI consulta msm_get_row()/position()
 *    msm_stop(p);                 // pausa (canal continua, em silêncio)
 *    gfxa_stream_stop();          // libera o canal de áudio
 *    msm_destroy(p);
 * ══════════════════════════════════════════════════════════════════════════ */

/* ─── Constantes ───────────────────────────────────────────────────────── */

#define MSM_CHANNELS           4
#define MSM_MAX_INSTRUMENTS   32
#define MSM_MAX_PATTERNS      64
#define MSM_MAX_SEQUENCE     128
#define MSM_ROWS              64
#define MSM_SPEED_DEFAULT      6
#define MSM_BPM_DEFAULT      125

#define MSM_NOTE_NONE  0xFF    /* nenhuma nota na célula */
#define MSM_NOTE_CUT   0xFE    /* corta o canal imediatamente */
#define MSM_NOTE_OFF   0xFD    /* solta a nota (fade rápido) */
#define MSM_INST_NONE  0xFF    /* célula sem instrumento */

/* Efeitos (coluna fx, estilo ProTracker) */
#define MSM_FX_ARPEGGIO    0x0  /* 0xy: alterna nota, +x, +y semitons     */
#define MSM_FX_PORTA_UP    0x1  /* 1xx: desliza pitch para cima           */
#define MSM_FX_PORTA_DOWN  0x2  /* 2xx: desliza pitch para baixo          */
#define MSM_FX_TONE_PORTA  0x3  /* 3xx: desliza até a nota da célula      */
#define MSM_FX_VIBRATO     0x4  /* 4xy: x=velocidade y=profundidade       */
#define MSM_FX_TREMOLO     0x7  /* 7xy: x=velocidade y=profundidade (vol) */
#define MSM_FX_VOL_SLIDE   0xA  /* Axy: x=sobe y=desce (por tick)         */
#define MSM_FX_JUMP        0xB  /* Bxx: salta para posição xx da sequência*/
#define MSM_FX_VOLUME      0xC  /* Cxx: volume do canal 0..64 (0x40)      */
#define MSM_FX_BREAK       0xD  /* Dxx: próximo pattern, começa na row xx */
#define MSM_FX_EXTENDED    0xE  /* ECx: corta no tick x; EDx: atrasa nota */
#define MSM_FX_SPEED       0xF  /* Fxx: xx<0x20 speed(ticks/row), senão BPM */

/* ─── Estruturas de dados (formato .MSM) ──────────────────────────────── */

/* Uma célula num pattern — 4 bytes */
typedef struct {
  uint8_t note;        /* 0..119 = nota MIDI, MSM_NOTE_* para especiais */
  uint8_t instrument;  /* 0..31, MSM_INST_NONE = sem mudança */
  uint8_t effect;      /* 0..15 */
  uint8_t param;       /* parâmetro do efeito */
} msm_cell_t;

/* Pattern: 64 rows × 4 canais */
typedef struct {
  msm_cell_t cell[MSM_ROWS][MSM_CHANNELS];
} msm_pattern_t;

/* Instrumento: parâmetros sfxr compactados + nome */
typedef struct {
  uint8_t params[GFXA_SFXR_PARAM_COUNT];  /* gfxa_sfxr_pack() */
  char    name[12];
} msm_instrument_t;

/* Cabeçalho do .MSM (formato binário em disco, 40 bytes, só uint8) */
typedef struct {
  char     magic[4];           /* "MSM1" */
  char     name[22];           /* nome da música, zero-padded */
  uint8_t  speed;              /* ticks por row */
  uint8_t  pattern_count;
  uint8_t  instrument_count;
  uint8_t  sequence_length;
  uint8_t  channels;           /* 4 */
  uint8_t  rows_per_pattern;   /* 64 */
  uint8_t  bpm;                /* tempo (tick = 2.5/bpm s); 0 = 125 */
  uint8_t  restart;            /* posição de loop ao fim da sequência */
  uint8_t  reserved[6];
} msm_header_t;

/* Música completa em memória (~70 KB com tudo cheio — prefira alocar) */
typedef struct {
  msm_header_t      header;
  uint8_t           sequence[MSM_MAX_SEQUENCE];
  msm_instrument_t  instruments[MSM_MAX_INSTRUMENTS];
  msm_pattern_t     patterns[MSM_MAX_PATTERNS];
} msm_song_t;

/* ─── Song helpers ─────────────────────────────────────────────────────── */

/* Inicializa uma música vazia: 1 pattern, sequência {0}, speed/bpm
 * default e 8 instrumentos chiptune padrão. */
void msm_song_init(msm_song_t *song, const char *name);

/* Preenche instruments[0..7] com o banco chiptune padrão
 * (square lead, saw bass, etc). Não altera o resto da música. */
void msm_default_instruments(msm_song_t *song);

/* ─── Player ───────────────────────────────────────────────────────────── */

typedef struct msm_player msm_player_t;

/* Cria um player para a música. A song não é copiada — precisa permanecer
 * válida enquanto o player existir. O player escreve em song apenas via
 * msm_* (nunca sozinho); a UI pode editar células livremente. */
msm_player_t *msm_create(msm_song_t *song);
void msm_destroy(msm_player_t *p);

/* Callback de áudio (audio_fill_fn). userdata = msm_player_t*.
 * Sempre preenche n samples (silêncio quando parado) e retorna n,
 * então o canal do engine permanece vivo entre play/stop. */
int msm_fill(int16_t *buf, int n, void *userdata);

/* Transporte */
void msm_play(msm_player_t *p);                      /* do início */
void msm_play_from(msm_player_t *p, int pos, int row);
void msm_stop(msm_player_t *p);                      /* para e silencia */
bool msm_is_playing(const msm_player_t *p);

/* Loop: 0 = música inteira (volta em header.restart, default),
 *       1 = repete apenas o pattern atual (útil no editor) */
void msm_set_loop_pattern(msm_player_t *p, bool loop_pattern);
bool msm_get_loop_pattern(const msm_player_t *p);

/* Posição (UI pode consultar a qualquer momento) */
int  msm_get_position(const msm_player_t *p);
int  msm_get_row(const msm_player_t *p);
int  msm_get_tick(const msm_player_t *p);
int  msm_get_current_pattern(const msm_player_t *p);
void msm_set_position(msm_player_t *p, int pos);

/* Velocidade (espelha Fxx) */
int  msm_get_speed(const msm_player_t *p);
void msm_set_speed(msm_player_t *p, int speed);
int  msm_get_bpm(const msm_player_t *p);
void msm_set_bpm(msm_player_t *p, int bpm);

/* Volume global 0.0..1.0 */
void  msm_set_global_volume(msm_player_t *p, float vol);
float msm_get_global_volume(const msm_player_t *p);

/* Mute por canal (para compor) */
void msm_set_mute(msm_player_t *p, int ch, bool mute);
bool msm_get_mute(const msm_player_t *p, int ch);

/* Dispara uma nota ao vivo no canal (jam/preview do editor).
 * note também aceita MSM_NOTE_CUT. Thread-safe vs. o áudio. */
void msm_jam(msm_player_t *p, int ch, int note, int instrument);

/* Última nota tocada num canal (-1 se mudo/inativo) — para VU/visual */
int msm_get_channel_note(const msm_player_t *p, int ch);

/* Pausa o sequenciador e o consumo da song pelo áudio enquanto a UI faz
 * mudanças estruturais (load, resize). Sempre pareie lock/unlock. */
void msm_edit_lock(msm_player_t *p);
void msm_edit_unlock(msm_player_t *p);

/* ─── Serialização ─────────────────────────────────────────────────────── */

/* Stream para arquivo. Retornam 0 em sucesso. */
int msm_save_file(const msm_song_t *song, FILE *f);
int msm_load_file(msm_song_t *song, FILE *f);

/* Buffer binário (mesmo layout do arquivo). pack retorna bytes escritos
 * (0 se não coube); unpack retorna 0 em sucesso. */
int msm_pack(const msm_song_t *song, uint8_t *buf, int buf_size);
int msm_unpack(const uint8_t *buf, int buf_size, msm_song_t *song);

/* Importa um ProTracker .MOD de 4 canais (M.K.). Os samples são
 * ignorados: cada sample vira um instrumento do banco chiptune padrão
 * (com o nome original), notas e efeitos compatíveis são convertidos.
 * Retorna 0 em sucesso. */
int msm_import_mod(const uint8_t *buf, int size, msm_song_t *song);

/* Converte nota MIDI → frequência Hz (A4 = 69 = 440 Hz) */
float msm_midi_to_freq(int midi_note);

#ifdef __cplusplus
}
#endif
