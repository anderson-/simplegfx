#pragma once
/* ══════════════════════════════════════════════════════════════════════════
 *  tracker_app.h — MSM Tracker: editor/compositor de chiptune (4 canais)
 *
 *  App completo e portátil (desktop SDL/macOS, rg35xx, M5 Cardputer).
 *  Inclua este header em UM arquivo do app; ele define os callbacks
 *  gfx_app / gfx_draw / gfx_on_key / gfx_process_data.
 *
 *  Desenhado para a grade de 40×16 caracteres do Cardputer (240×135,
 *  fonte 5×7); em telas maiores usa fonte 2x.
 *
 *  Controles (pattern):
 *    setas (Fn+,;./ no Cardputer)  move o cursor
 *    SPACE       liga/desliga modo de edição
 *    ENTER / a   toca/para a música na posição atual
 *    l           repete só o pattern atual (loop)
 *    TAB         próxima tela   |   ESC  menu
 *    ;  '        posição anterior/seguinte da sequência
 *    [  ]        oitava   |   -  =   instrumento
 *    !  @  #  $  muta canal 1..4
 *    Em edição: piano estilo ProTracker (zsxdcvgbhnjm / q2w3er5t6y7u),
 *    1 = note-off, \ = note-cut, . ou BACKSPACE = apaga,
 *    0-9 a-f = valores nas colunas inst/fx/param
 * ══════════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "simplegfx.h"
#include "audio_engine.h"
#include "sfxr.h"
#include "sfxr_presets.h"
#include "modtracker.h"

#ifdef ESP32
#define TA_DIR "/sdcard"
#else
#define TA_DIR "."
#endif

/* Teclas de seta: no Cardputer o HAL envia 17..20; nos backends desktop
 * valem os BTN_KB_* do keymap.h. */
#if defined(GFX_SDL) || defined(GFX_SDL2) || defined(GFX_MACOS)
#define TA_KEY_UP    BTN_KB_UP
#define TA_KEY_DOWN  BTN_KB_DOWN
#define TA_KEY_LEFT  BTN_KB_LEFT
#define TA_KEY_RIGHT BTN_KB_RIGHT
#else
#define TA_KEY_UP    ((char)17)
#define TA_KEY_DOWN  ((char)18)
#define TA_KEY_LEFT  ((char)19)
#define TA_KEY_RIGHT ((char)20)
#endif

#define TA_ESC  ((char)27)
#define TA_TAB  ((char)9)

/* ─── Estado ───────────────────────────────────────────────────────────── */

enum { TA_SCR_PATTERN, TA_SCR_INST, TA_SCR_SONG, TA_SCR_FILE, TA_SCR_HELP };

static msm_song_t   *ta_song   = NULL;
static msm_player_t *ta_player = NULL;

static int ta_fs, ta_cw, ta_chh;      /* fonte: tamanho, célula px        */
static int ta_cols, ta_rows;          /* grade de caracteres              */

static int ta_screen = TA_SCR_PATTERN;
static int ta_menu = -1;              /* item do menu (-1 = fechado)      */
static int ta_edit = 0;
static int ta_octave = 4;
static int ta_inst = 0;               /* instrumento corrente p/ entrada  */
static int ta_pos = 0;                /* posição exibida da sequência     */
static int ta_cur_row = 0, ta_cur_ch = 0, ta_cur_col = 0;
static int ta_confirm_new = 0;

static int ta_inst_sel = 0, ta_inst_scroll = 0;

static int ta_song_sel = 0, ta_song_scroll = 0;

#define TA_MAX_FILES 24
static char ta_files[TA_MAX_FILES][28];
static int  ta_file_count = 0, ta_file_sel = 0, ta_file_scroll = 0;
static char ta_savename[20] = "song";
static int  ta_saving = 0;

static char ta_msg[44] = "";
static int  ta_msg_t = 0;

/* ─── Pequenos helpers de UI ───────────────────────────────────────────── */

static void ta_text(int col, int row, const char *s) {
  gfx_text(s, col * ta_cw, row * ta_chh, ta_fs);
}

static void ta_textf(int col, int row, const char *fmt, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  ta_text(col, row, buf);
}

static void ta_fill(int col, int row, int w, int h) {
  gfx_fill_rect(col * ta_cw, row * ta_chh, w * ta_cw, h * ta_chh);
}

static void ta_message(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ta_msg, sizeof(ta_msg), fmt, ap);
  va_end(ap);
  ta_msg_t = 90;
}

static const char *ta_note_name(int note) {
  static const char names[12][3] = {
    "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
  };
  static char buf[4];
  if (note == MSM_NOTE_NONE) return "...";
  if (note == MSM_NOTE_OFF)  return "===";
  if (note == MSM_NOTE_CUT)  return "^^^";
  if (note > 119) return "???";
  snprintf(buf, sizeof(buf), "%s%d", names[note % 12], note / 12 - 1);
  return buf;
}

static char ta_b32(int v) {   /* instrumento em 1 caractere (0-9a-v) */
  if (v < 10) return (char)('0' + v);
  return (char)('a' + v - 10);
}

/* ─── Piano (layout ProTracker) ────────────────────────────────────────── */

static int ta_piano_semi(char k) {
  static const char low[]  = "zsxdcvgbhnjm";       /* C..B  (oitava)     */
  static const char high[] = "q2w3er5t6y7u";       /* C..B  (oitava + 1) */
  static const char top[]  = "i9o0p";              /* C..E  (oitava + 2) */
  const char *p;
  if ((p = strchr(low, k)) != NULL && k)  return (int)(p - low);
  if ((p = strchr(high, k)) != NULL && k) return 12 + (int)(p - high);
  if ((p = strchr(top, k)) != NULL && k)  return 24 + (int)(p - top);
  return -1;
}

static int ta_hex_digit(char k) {
  if (k >= '0' && k <= '9') return k - '0';
  if (k >= 'a' && k <= 'f') return k - 'a' + 10;
  return -1;
}

static int ta_b32_digit(char k) {
  if (k >= '0' && k <= '9') return k - '0';
  if (k >= 'a' && k <= 'v') return k - 'a' + 10;
  return -1;
}

/* ─── Acesso à música ──────────────────────────────────────────────────── */

static int ta_view_pos(void) {
  if (msm_is_playing(ta_player)) return msm_get_position(ta_player);
  return ta_pos;
}

static int ta_view_pattern(void) {
  int pos = ta_view_pos();
  if (pos >= ta_song->header.sequence_length) pos = 0;
  int pat = ta_song->sequence[pos];
  return pat < ta_song->header.pattern_count ? pat : 0;
}

static msm_cell_t *ta_cell(int row, int ch) {
  return &ta_song->patterns[ta_view_pattern()].cell[row][ch];
}

static void ta_clear_pattern(msm_pattern_t *p) {
  for (int r = 0; r < MSM_ROWS; r++)
    for (int c = 0; c < MSM_CHANNELS; c++) {
      p->cell[r][c].note = MSM_NOTE_NONE;
      p->cell[r][c].instrument = MSM_INST_NONE;
      p->cell[r][c].effect = 0;
      p->cell[r][c].param = 0;
    }
}

/* Garante que o pattern n existe (cria vazio se for o próximo). */
static void ta_ensure_pattern(int n) {
  if (n < ta_song->header.pattern_count) return;
  if (n >= MSM_MAX_PATTERNS) return;
  msm_edit_lock(ta_player);
  for (int i = ta_song->header.pattern_count; i <= n; i++)
    ta_clear_pattern(&ta_song->patterns[i]);
  ta_song->header.pattern_count = (uint8_t)(n + 1);
  msm_edit_unlock(ta_player);
}

/* ─── Demo song ────────────────────────────────────────────────────────── */

static void ta_put(int pat, int row, int ch, int note, int inst,
                   int fx, int param) {
  msm_cell_t *c = &ta_song->patterns[pat].cell[row][ch];
  c->note = (uint8_t)note;
  c->instrument = (uint8_t)inst;
  c->effect = (uint8_t)fx;
  c->param = (uint8_t)param;
}

/* Instrumentos do banco padrão (msm_default_instruments):
 *   0 SqLead  1 SqThin  2 SawBass  3 SinePad  4 Kick  5 Snare  6 HiHat
 *   7 Pluck
 * Progressão (1 pattern = 4 compassos de 16 rows): Am F C G            */
static void ta_build_demo(void) {
  msm_song_init(ta_song, "axis demo");
  ta_song->header.pattern_count = 4;
  for (int i = 1; i < 4; i++) ta_clear_pattern(&ta_song->patterns[i]);
  ta_clear_pattern(&ta_song->patterns[0]);

  static const int root[4] = { 45, 41, 48, 43 };        /* A2 F2 C3 G2 */
  static const int arpn[4] = { 57, 53, 60, 55 };        /* A3 F3 C4 G3 */
  static const int arpp[4] = { 0x37, 0x47, 0x47, 0x47 };/* m / M / M / M */

  /* ── P0/P1/P2: base de bateria + baixo ── */
  for (int pat = 0; pat <= 2; pat++) {
    for (int bar = 0; bar < 4; bar++) {
      int b = bar * 16;
      /* bateria (ch3): bumbo/chimbal/caixa */
      ta_put(pat, b + 0,  3, 36, 4, 0, 0);
      ta_put(pat, b + 2,  3, 69, 6, MSM_FX_VOLUME, 32);
      ta_put(pat, b + 4,  3, 50, 5, 0, 0);
      ta_put(pat, b + 6,  3, 69, 6, MSM_FX_VOLUME, 32);
      ta_put(pat, b + 8,  3, 36, 4, 0, 0);
      ta_put(pat, b + 10, 3, 69, 6, MSM_FX_VOLUME, 32);
      ta_put(pat, b + 12, 3, 50, 5, 0, 0);
      ta_put(pat, b + 14, 3, 69, 6, MSM_FX_VOLUME, 32);
      /* baixo (ch0): colcheias na fundamental + cromatismos */
      for (int r = 0; r < 16; r += 2) {
        int note = root[bar];
        if (r == 6)  note += 12;
        if (r == 14) note += 7;
        ta_put(pat, b + r, 0, note, 2, 0, 0);
      }
    }
  }
  /* virada no fim do P0 e P2 */
  ta_put(0, 61, 3, 50, 5, 0, 0);
  ta_put(0, 62, 3, 50, 5, 0, 0);
  ta_put(0, 63, 3, 50, 5, MSM_FX_VOLUME, 48);
  ta_put(2, 60, 3, 50, 5, 0, 0);
  ta_put(2, 61, 3, 50, 5, MSM_FX_VOLUME, 40);
  ta_put(2, 62, 3, 50, 5, 0, 0);
  ta_put(2, 63, 3, 50, 5, MSM_FX_VOLUME, 56);

  /* ── P1/P2: arpejos (ch2, Pluck com efeito 0xy) ── */
  for (int pat = 1; pat <= 2; pat++)
    for (int bar = 0; bar < 4; bar++)
      for (int r = 0; r < 16; r += 4)
        ta_put(pat, bar * 16 + r, 2, arpn[bar], 7, MSM_FX_ARPEGGIO, arpp[bar]);

  /* ── P2: melodia (ch1, SqLead) ── */
  {
    static const int mel[][4] = {  /* bar, row, nota, fx<<8|param */
      {0,  0, 76, 0},        {0,  4, 72, 0},  {0,  6, 74, 0},
      {0,  8, 76, 0x0443},   {0, 14, 74, 0},
      {1,  0, 72, 0},        {1,  4, 69, 0},  {1,  6, 67, 0},
      {1,  8, 69, 0x0443},   {1, 12, 72, 0},
      {2,  0, 72, 0},        {2,  4, 76, 0},
      {2,  8, 79, 0x0443},   {2, 12, 76, 0},
      {3,  0, 74, 0},        {3,  4, 71, 0},
      {3,  8, 74, 0x0443},   {3, 12, 71, 0},  {3, 14, 72, 0},
    };
    for (unsigned i = 0; i < sizeof(mel) / sizeof(mel[0]); i++)
      ta_put(2, mel[i][0] * 16 + mel[i][1], 1, mel[i][2], 0,
             (mel[i][3] >> 8) & 0xF, mel[i][3] & 0xFF);
  }

  /* ── P3: quebra — pads, slides de baixo e bateria rala ── */
  for (int bar = 0; bar < 4; bar++) {
    int b = bar * 16;
    /* baixo: nota limpa, retriggers e slide (3xx) até a fundamental
     * do compasso seguinte enquanto a voz ainda soa */
    ta_put(3, b + 0,  0, root[bar], 2, 0, 0);
    ta_put(3, b + 8,  0, root[bar], 2, 0, 0);
    ta_put(3, b + 12, 0, root[bar], 2, 0, 0);
    ta_put(3, b + 14, 0, root[(bar + 1) % 4], 2, MSM_FX_TONE_PORTA, 0x40);
    ta_put(3, b,     1, arpn[bar], 3, MSM_FX_ARPEGGIO, arpp[bar]);
    ta_put(3, b + 8, 1, arpn[bar], 3, MSM_FX_ARPEGGIO, arpp[bar]);
    ta_put(3, b, 3, 36, 4, 0, 0);
    for (int r = 0; r < 16; r += 2)
      if (r != 0)
        ta_put(3, b + r, 3, 69, 6, MSM_FX_VOLUME, 20);
  }
  ta_put(3, 12, 2, 81, 7, MSM_FX_VOLUME, 36);
  ta_put(3, 28, 2, 77, 7, MSM_FX_VOLUME, 36);
  ta_put(3, 44, 2, 76, 7, MSM_FX_VOLUME, 36);
  ta_put(3, 60, 2, 74, 7, MSM_FX_VOLUME, 36);

  /* sequência: intro, verso, refrão ×2, quebra, verso, refrão ×2 */
  static const uint8_t seq[8] = { 0, 1, 2, 2, 3, 1, 2, 2 };
  memcpy(ta_song->sequence, seq, sizeof(seq));
  ta_song->header.sequence_length = 8;
  ta_song->header.restart = 1;
}

/* ─── Arquivos ─────────────────────────────────────────────────────────── */

static void ta_scan_files(void) {
  ta_file_count = 0;
  ta_file_sel = 0;
  ta_file_scroll = 0;
  DIR *d = opendir(TA_DIR);
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d)) != NULL && ta_file_count < TA_MAX_FILES) {
    const char *n = e->d_name;
    size_t len = strlen(n);
    if (len < 5 || len >= sizeof(ta_files[0])) continue;
    const char *ext = n + len - 4;
    if (strcmp(ext, ".msm") && strcmp(ext, ".MSM") &&
        strcmp(ext, ".mod") && strcmp(ext, ".MOD")) continue;
    snprintf(ta_files[ta_file_count++], sizeof(ta_files[0]), "%s", n);
  }
  closedir(d);
}

static void ta_save_song(void) {
  char path[64];
  snprintf(path, sizeof(path), TA_DIR "/%s.msm", ta_savename);
  memset(ta_song->header.name, 0, sizeof(ta_song->header.name));
  snprintf(ta_song->header.name, sizeof(ta_song->header.name), "%s",
           ta_savename);
  FILE *f = fopen(path, "wb");
  if (!f) { ta_message("erro ao criar %s", path); return; }
  int err = msm_save_file(ta_song, f);
  fclose(f);
  ta_message(err ? "erro ao salvar!" : "salvo: %s.msm", ta_savename);
  ta_scan_files();
}

static void ta_load_song(const char *name) {
  char path[64];
  snprintf(path, sizeof(path), TA_DIR "/%s", name);
  FILE *f = fopen(path, "rb");
  if (!f) { ta_message("erro ao abrir %s", name); return; }

  msm_stop(ta_player);
  msm_edit_lock(ta_player);

  int err;
  size_t len = strlen(name);
  if (len > 4 && (!strcmp(name + len - 4, ".mod") ||
                  !strcmp(name + len - 4, ".MOD"))) {
    /* MOD: lê header, calcula patterns, lê só o necessário */
    int max = 1084 + MSM_MAX_PATTERNS * 1024;
    uint8_t *buf = (uint8_t *)malloc((size_t)max);
    if (buf) {
      int got = (int)fread(buf, 1, (size_t)max, f);
      err = msm_import_mod(buf, got, ta_song);
      free(buf);
    } else err = 1;
  } else {
    err = msm_load_file(ta_song, f);
  }
  fclose(f);

  if (err) {
    ta_build_demo();   /* song pode ter ficado inconsistente */
    ta_message("arquivo invalido!");
  } else {
    ta_message("carregado: %s", name);
    /* nome do arquivo vira sugestão de save */
    snprintf(ta_savename, sizeof(ta_savename), "%.*s",
             (int)(len - 4), name);
  }
  ta_pos = 0; ta_cur_row = 0; ta_cur_ch = 0; ta_cur_col = 0;
  ta_inst = 0; ta_inst_sel = 0; ta_song_sel = 0;
  msm_edit_unlock(ta_player);
}

/* ─── Transporte ───────────────────────────────────────────────────────── */

static void ta_toggle_play(void) {
  if (msm_is_playing(ta_player)) {
    ta_pos = msm_get_position(ta_player);
    msm_stop(ta_player);
  } else {
    msm_play_from(ta_player, ta_pos, 0);
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Telas — desenho
 * ══════════════════════════════════════════════════════════════════════════ */

static void ta_draw_pattern(void) {
  const msm_header_t *h = &ta_song->header;
  int playing = msm_is_playing(ta_player);
  int pos = ta_view_pos();
  int pat = ta_view_pattern();
  int prow = playing ? msm_get_row(ta_player) : -1;

  /* linha 0: status */
  gfx_set_color(80, 220, 180);
  ta_textf(0, 0, "%02d/%02d:%02d", pos, h->sequence_length - 1, pat);
  gfx_set_color(150, 150, 170);
  ta_textf(9, 0, "%d/%d", msm_get_speed(ta_player), msm_get_bpm(ta_player));
  gfx_set_color(220, 180, 90);
  ta_textf(15, 0, "O%d i%c", ta_octave, ta_b32(ta_inst));
  if (ta_edit) {
    gfx_set_color(250, 90, 90);
    ta_text(21, 0, "EDIT");
  }
  if (msm_get_loop_pattern(ta_player)) {
    gfx_set_color(180, 120, 250);
    ta_text(26, 0, "LP");
  }
  gfx_set_color(playing ? 90 : 120, playing ? 240 : 120, 90);
  ta_text(29, 0, playing ? ">" : "#");
  if (ta_fs > 1 || ta_cols > 41) {
    gfx_set_color(120, 130, 150);
    ta_textf(31, 0, "%s", h->name);
  }

  /* linha 1: cabeçalho dos canais (nota ativa + mute) */
  for (int c = 0; c < MSM_CHANNELS; c++) {
    int x = 3 + c * 9;
    if (msm_get_mute(ta_player, c)) {
      gfx_set_color(220, 80, 80);
      ta_textf(x, 1, "%d:MUTE", c + 1);
    } else {
      gfx_set_color(90, 110, 140);
      ta_textf(x, 1, "%d:%s", c + 1, ta_note_name(
        msm_get_channel_note(ta_player, c) < 0 ? MSM_NOTE_NONE
        : msm_get_channel_note(ta_player, c)));
    }
  }

  /* grade do pattern */
  int top = 2;
  int vis = ta_rows - top - 1;
  int center = playing ? prow : ta_cur_row;
  int scroll = center - vis / 2;
  if (scroll < 0) scroll = 0;
  if (scroll > MSM_ROWS - vis) scroll = MSM_ROWS - vis;
  if (vis >= MSM_ROWS) scroll = 0;

  for (int i = 0; i < vis && scroll + i < MSM_ROWS; i++) {
    int row = scroll + i;
    int y = top + i;

    if (row == prow) {
      gfx_set_color(40, 70, 45);
      ta_fill(0, y, 3 + 9 * MSM_CHANNELS, 1);
    } else if (row == ta_cur_row && !playing) {
      gfx_set_color(38, 40, 62);
      ta_fill(0, y, 3 + 9 * MSM_CHANNELS, 1);
    }

    gfx_set_color(row % 4 ? 95 : 140, row % 4 ? 85 : 130, 115);
    ta_textf(0, y, "%02X", row);

    for (int c = 0; c < MSM_CHANNELS; c++) {
      const msm_cell_t *cell = ta_cell(row, c);
      int x = 3 + c * 9;

      /* cursor */
      if (row == ta_cur_row && c == ta_cur_ch) {
        static const int off[5] = { 0, 4, 5, 6, 7 };
        static const int wid[5] = { 3, 1, 1, 1, 1 };
        gfx_set_color(ta_edit ? 150 : 90, ta_edit ? 60 : 80,
                      ta_edit ? 60 : 140);
        ta_fill(x + off[ta_cur_col], y, wid[ta_cur_col], 1);
      }

      if (cell->note == MSM_NOTE_NONE)
        gfx_set_color(70, 72, 96);
      else if (cell->note >= 120)
        gfx_set_color(200, 120, 80);
      else
        gfx_set_color(140, 230, 140);
      ta_text(x, y, ta_note_name(cell->note));

      if (cell->instrument != MSM_INST_NONE) {
        gfx_set_color(230, 210, 110);
        ta_textf(x + 4, y, "%c", ta_b32(cell->instrument));
      } else {
        gfx_set_color(70, 72, 96);
        ta_text(x + 4, y, ".");
      }

      if (cell->effect || cell->param) {
        gfx_set_color(120, 180, 240);
        ta_textf(x + 5, y, "%X%02X", cell->effect, cell->param);
      } else {
        gfx_set_color(70, 72, 96);
        ta_text(x + 5, y, "...");
      }
    }
  }
}

/* ── Instrumentos ── */

static const char *ta_param_names[GFXA_SFXR_PARAM_COUNT] = {
  "Wave", "Attack", "Sustain", "Punch", "Decay", "BaseFreq", "FreqLimit",
  "FreqRamp", "FreqDRamp", "VibDepth", "VibSpeed", "ArpMod", "ArpSpeed",
  "Duty", "DutyRamp", "Repeat", "PhaOffset", "PhaRamp", "LPF", "LPFRamp",
  "LPFRes", "HPF", "HPFRamp", "Volume"
};

static void ta_draw_inst(void) {
  msm_instrument_t *ins = &ta_song->instruments[ta_inst];

  gfx_set_color(80, 220, 180);
  ta_textf(0, 0, "INST %c/%c", ta_b32(ta_inst),
           ta_b32(ta_song->header.instrument_count - 1));
  gfx_set_color(230, 210, 110);
  ta_textf(10, 0, "%s", ins->name);

  int vis = ta_rows - 2;
  if (ta_inst_sel < ta_inst_scroll) ta_inst_scroll = ta_inst_sel;
  if (ta_inst_sel >= ta_inst_scroll + vis)
    ta_inst_scroll = ta_inst_sel - vis + 1;

  for (int i = 0; i < vis && ta_inst_scroll + i < GFXA_SFXR_PARAM_COUNT; i++) {
    int idx = ta_inst_scroll + i;
    int y = 1 + i;
    int v = ins->params[idx];

    if (idx == ta_inst_sel) {
      gfx_set_color(50, 55, 85);
      ta_fill(0, y, ta_cols, 1);
    }
    gfx_set_color(idx == ta_inst_sel ? 250 : 170,
                  idx == ta_inst_sel ? 250 : 170, 190);
    ta_text(1, y, ta_param_names[idx]);

    if (idx == GFXA_SFXR_WAVE_TYPE) {
      static const char *w[4] = { "SQUARE", "SAW", "SINE", "NOISE" };
      int wt = (v * 3 + 127) / 255;
      gfx_set_color(140, 230, 140);
      ta_textf(12, y, "%s", w[wt & 3]);
    } else {
      gfx_set_color(140, 230, 140);
      ta_textf(12, y, "%4.2f", (float)v / 255.0f);
      gfx_set_color(70, 110, 160);
      int bar = v * 18 / 255;
      if (bar > 0) ta_fill(18, y, bar > 18 ? 18 : bar, 1);
    }
  }

  gfx_set_color(90, 100, 120);
  ta_text(0, ta_rows - 1,
          ta_cols > 41
            ? "</>=val -/==val16 [/]=inst ,/.=preset /=rnd \\=mut ENT=toca"
            : "<> -= val  [] inst  ,. pre  / \\");
}

/* ── Song / sequência ── */

static void ta_draw_song(void) {
  msm_header_t *h = &ta_song->header;
  gfx_set_color(80, 220, 180);
  ta_textf(0, 0, "SONG  %s", h->name);

  static const char *fields[3] = { "Speed", "BPM", "Restart" };
  int fv[3] = { h->speed, h->bpm, h->restart };
  for (int i = 0; i < 3; i++) {
    if (ta_song_sel == i) {
      gfx_set_color(50, 55, 85);
      ta_fill(0, 1 + i, 14, 1);
    }
    gfx_set_color(170, 170, 190);
    ta_text(1, 1 + i, fields[i]);
    gfx_set_color(140, 230, 140);
    ta_textf(10, 1 + i, "%3d", fv[i]);
  }

  gfx_set_color(90, 110, 140);
  ta_textf(0, 4, "SEQ %d/%d  PAT %d", h->sequence_length,
           MSM_MAX_SEQUENCE, h->pattern_count);

  int vis = ta_rows - 6;
  int sel = ta_song_sel - 3;
  if (sel < 0) sel = 0;
  if (sel < ta_song_scroll) ta_song_scroll = sel;
  if (sel >= ta_song_scroll + vis) ta_song_scroll = sel - vis + 1;

  for (int i = 0; i < vis && ta_song_scroll + i < h->sequence_length; i++) {
    int idx = ta_song_scroll + i;
    int y = 5 + i;
    int cur = (ta_song_sel - 3 == idx);
    int playing_here = msm_is_playing(ta_player) &&
                       msm_get_position(ta_player) == idx;
    if (cur) {
      gfx_set_color(50, 55, 85);
      ta_fill(0, y, 14, 1);
    }
    gfx_set_color(playing_here ? 90 : 120, playing_here ? 240 : 120,
                  playing_here ? 90 : 140);
    ta_textf(1, y, "%02d", idx);
    gfx_set_color(cur ? 250 : 170, cur ? 250 : 170, 190);
    ta_textf(5, y, ": %02d", ta_song->sequence[idx]);
  }

  gfx_set_color(90, 100, 120);
  ta_text(0, ta_rows - 1, "</> valor  i=ins x=del ENT=toca");
}

/* ── Arquivos ── */

static void ta_draw_file(void) {
  gfx_set_color(80, 220, 180);
  ta_textf(0, 0, "FILES %s", TA_DIR);

  /* entrada 0 = salvar */
  if (ta_file_sel == 0) {
    gfx_set_color(50, 55, 85);
    ta_fill(0, 1, ta_cols, 1);
  }
  gfx_set_color(ta_saving ? 250 : 200, 180, 90);
  ta_textf(1, 1, "[salvar: %s%s.msm]", ta_savename, ta_saving ? "_" : "");

  int vis = ta_rows - 3;
  int sel = ta_file_sel - 1;
  if (sel < 0) sel = 0;
  if (sel < ta_file_scroll) ta_file_scroll = sel;
  if (sel >= ta_file_scroll + vis) ta_file_scroll = sel - vis + 1;

  for (int i = 0; i < vis && ta_file_scroll + i < ta_file_count; i++) {
    int idx = ta_file_scroll + i;
    int y = 2 + i;
    if (ta_file_sel - 1 == idx) {
      gfx_set_color(50, 55, 85);
      ta_fill(0, y, ta_cols, 1);
    }
    gfx_set_color(170, 200, 230);
    ta_textf(1, y, "%s", ta_files[idx]);
  }
  if (!ta_file_count) {
    gfx_set_color(110, 110, 130);
    ta_text(1, 3, "(nenhum .msm/.mod)");
  }

  gfx_set_color(90, 100, 120);
  ta_text(0, ta_rows - 1, "ENT=carrega/salva  ESC=volta");
}

/* ── Ajuda ── */

static void ta_draw_help(void) {
  static const char *lines[] = {
    "MSM TRACKER  - ajuda",
    "TAB telas  ESC menu  SPC edicao",
    "ENTER/a toca/para  l loop pattern",
    "setas move  ; ' posicao seq",
    "[ ] oitava  - = instrumento",
    "! @ # $ muta canal 1-4",
    "piano: zsxdcvgbhnjm  q2w3er5t6y7u",
    "1 note-off  \\ cut  . apaga",
    "colunas: nota inst fx param",
    "fx: 0 arp 1/2 porta 3 tone-porta",
    "    4 vib 7 trem A volslide",
    "    B salto C vol D break",
    "    ECx corta EDx atrasa Fxx spd",
    "salve .msm; importa .mod 4ch",
  };
  for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
    gfx_set_color(i ? 170 : 80, i ? 170 : 220, i ? 190 : 180);
    if ((int)i < ta_rows) ta_text(0, (int)i, lines[i]);
  }
}

/* ── Menu (overlay) ── */

static const char *ta_menu_items[] = {
  "continuar", "tocar/parar", "loop pattern", "patterns",
  "instrumentos", "song/sequencia", "arquivos", "ajuda",
  "nova musica", "sair",
};
#define TA_MENU_N ((int)(sizeof(ta_menu_items) / sizeof(ta_menu_items[0])))

static void ta_draw_menu(void) {
  int w = 20, h = TA_MENU_N + 2;
  int x = (ta_cols - w) / 2, y = (ta_rows - h) / 2;
  if (y < 0) y = 0;
  gfx_set_color(25, 28, 45);
  ta_fill(x, y, w, h);
  gfx_set_color(80, 220, 180);
  ta_text(x + 1, y, "== MENU ==");
  for (int i = 0; i < TA_MENU_N; i++) {
    if (i == ta_menu) {
      gfx_set_color(60, 70, 110);
      ta_fill(x, y + 1 + i, w, 1);
    }
    if (i == 8 && ta_confirm_new) gfx_set_color(250, 120, 90);
    else gfx_set_color(i == ta_menu ? 250 : 170, i == ta_menu ? 250 : 170, 190);
    ta_textf(x + 1, y + 1 + i, "%s%s", ta_menu_items[i],
             (i == 8 && ta_confirm_new) ? " (denovo!)" : "");
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Teclado
 * ══════════════════════════════════════════════════════════════════════════ */

static int ta_key_menu(char k) {
  if (k == TA_KEY_UP || k == 'w')   ta_menu = (ta_menu + TA_MENU_N - 1) % TA_MENU_N;
  if (k == TA_KEY_DOWN || k == 's') ta_menu = (ta_menu + 1) % TA_MENU_N;
  if (k == TA_ESC || k == 'b') { ta_menu = -1; ta_confirm_new = 0; }

  if (k == '\n' || k == '\r' || k == 'a' || k == 'l') {
    int item = ta_menu;
    if (item != 8) ta_confirm_new = 0;
    switch (item) {
      case 0: ta_menu = -1; break;
      case 1: ta_toggle_play(); ta_menu = -1; break;
      case 2:
        msm_set_loop_pattern(ta_player, !msm_get_loop_pattern(ta_player));
        ta_menu = -1;
        break;
      case 3: ta_screen = TA_SCR_PATTERN; ta_menu = -1; break;
      case 4: ta_screen = TA_SCR_INST; ta_menu = -1; break;
      case 5: ta_screen = TA_SCR_SONG; ta_menu = -1; break;
      case 6: ta_scan_files(); ta_screen = TA_SCR_FILE; ta_menu = -1; break;
      case 7: ta_screen = TA_SCR_HELP; ta_menu = -1; break;
      case 8:
        if (!ta_confirm_new) { ta_confirm_new = 1; break; }
        ta_confirm_new = 0;
        msm_stop(ta_player);
        msm_edit_lock(ta_player);
        msm_song_init(ta_song, "untitled");
        msm_edit_unlock(ta_player);
        ta_pos = 0; ta_cur_row = 0; ta_cur_ch = 0; ta_cur_col = 0;
        ta_message("nova musica");
        ta_menu = -1;
        ta_screen = TA_SCR_PATTERN;
        break;
      case 9: return 1;   /* sair */
    }
  }
  return 0;
}

static void ta_pattern_clear_at_cursor(void) {
  msm_cell_t *cell = ta_cell(ta_cur_row, ta_cur_ch);
  if (ta_cur_col == 0) {
    cell->note = MSM_NOTE_NONE;
    cell->instrument = MSM_INST_NONE;
  } else if (ta_cur_col == 1) {
    cell->instrument = MSM_INST_NONE;
  } else {
    cell->effect = 0;
    cell->param = 0;
  }
  if (ta_cur_row < MSM_ROWS - 1) ta_cur_row++;
}

static int ta_key_pattern(char k) {
  /* navegação */
  if (k == TA_KEY_UP   || (!ta_edit && k == 'w')) {
    ta_cur_row = (ta_cur_row + MSM_ROWS - 1) % MSM_ROWS; return 0;
  }
  if (k == TA_KEY_DOWN || (!ta_edit && k == 's')) {
    ta_cur_row = (ta_cur_row + 1) % MSM_ROWS; return 0;
  }
  if (k == TA_KEY_LEFT || (!ta_edit && k == 'q')) {
    if (--ta_cur_col < 0) {
      ta_cur_col = 4;
      ta_cur_ch = (ta_cur_ch + MSM_CHANNELS - 1) % MSM_CHANNELS;
    }
    return 0;
  }
  if (k == TA_KEY_RIGHT || (!ta_edit && k == 'd')) {
    if (++ta_cur_col > 4) {
      ta_cur_col = 0;
      ta_cur_ch = (ta_cur_ch + 1) % MSM_CHANNELS;
    }
    return 0;
  }

  /* posição da sequência */
  if (k == ';' || k == '\'') {
    int len = ta_song->header.sequence_length;
    ta_pos = (ta_pos + (k == ';' ? len - 1 : 1)) % len;
    if (msm_is_playing(ta_player)) msm_set_position(ta_player, ta_pos);
    return 0;
  }

  /* edição */
  if (ta_edit) {
    msm_cell_t *cell = ta_cell(ta_cur_row, ta_cur_ch);

    if (ta_cur_col == 0) {
      int semi = ta_piano_semi(k);
      if (semi >= 0) {
        int note = ta_octave * 12 + 12 + semi;   /* O4 → MIDI 60 (C4) */
        if (note > 119) note = 119;
        cell->note = (uint8_t)note;
        cell->instrument = (uint8_t)ta_inst;
        msm_jam(ta_player, ta_cur_ch, note, ta_inst);
        if (ta_cur_row < MSM_ROWS - 1) ta_cur_row++;
        return 0;
      }
      if (k == '1') {
        cell->note = MSM_NOTE_OFF;
        if (ta_cur_row < MSM_ROWS - 1) ta_cur_row++;
        return 0;
      }
      if (k == '\\') {
        cell->note = MSM_NOTE_CUT;
        if (ta_cur_row < MSM_ROWS - 1) ta_cur_row++;
        return 0;
      }
    }
    /* (colunas inst/fx/param são tratadas em ta_pattern_value_key) */

    if (k == '.' || k == 8 || k == 127) {
      ta_pattern_clear_at_cursor();
      return 0;
    }
  }

  return 0;
}

static int ta_key_inst(char k) {
  msm_instrument_t *ins = &ta_song->instruments[ta_inst];
  uint8_t *v = &ins->params[ta_inst_sel];

  if (k == TA_KEY_UP || k == 'w')
    ta_inst_sel = (ta_inst_sel + GFXA_SFXR_PARAM_COUNT - 1) % GFXA_SFXR_PARAM_COUNT;
  else if (k == TA_KEY_DOWN || k == 's')
    ta_inst_sel = (ta_inst_sel + 1) % GFXA_SFXR_PARAM_COUNT;
  else if (k == TA_KEY_LEFT || k == TA_KEY_RIGHT || k == 'q' || k == 'd' ||
           k == '-' || k == '=') {
    int delta = (k == TA_KEY_LEFT || k == 'q') ? -4 :
                (k == TA_KEY_RIGHT || k == 'd') ? 4 :
                (k == '-') ? -16 : 16;
    if (ta_inst_sel == GFXA_SFXR_WAVE_TYPE) {
      int wt = ((*v * 3 + 127) / 255 + (delta > 0 ? 1 : 3)) & 3;
      *v = (uint8_t)(wt * 255 / 3);
    } else {
      int nv = *v + delta;
      *v = (uint8_t)(nv < 0 ? 0 : nv > 255 ? 255 : nv);
    }
  }
  else if (k == '[' || k == ']') {
    int n = ta_song->header.instrument_count;
    if (k == ']' && ta_inst == n - 1 && n < MSM_MAX_INSTRUMENTS) {
      /* cria instrumento novo (cópia do atual) no fim */
      ta_song->instruments[n] = *ins;
      ta_song->header.instrument_count++;
      ta_inst = n;
      ta_message("novo instrumento %c", ta_b32(n));
    } else {
      ta_inst = (ta_inst + (k == '[' ? n - 1 : 1)) % n;
    }
  }
  else if (k == ',' || k == '.') {
    static int preset = 0;
    preset = (preset + (k == ',' ? GFXA_SFXR_PRESET_COUNT - 1 : 1))
             % GFXA_SFXR_PRESET_COUNT;
    float p[GFXA_SFXR_PARAM_COUNT];
    gfxa_sfxr_preset(preset, p);
    gfxa_sfxr_pack(p, ins->params);
    snprintf(ins->name, sizeof(ins->name), "%s",
             gfxa_sfxr_preset_name(preset));
  }
  else if (k == '/') {
    float p[GFXA_SFXR_PARAM_COUNT];
    gfxa_sfxr_random(p);
    gfxa_sfxr_pack(p, ins->params);
    snprintf(ins->name, sizeof(ins->name), "random");
  }
  else if (k == '\\') {
    float p[GFXA_SFXR_PARAM_COUNT];
    gfxa_sfxr_unpack(ins->params, p);
    gfxa_sfxr_mutate(p);
    gfxa_sfxr_pack(p, ins->params);
  }
  else if (k == '\n' || k == '\r' || k == 'a') {
    msm_jam(ta_player, 0, 60, ta_inst);   /* C-4 de preview */
  }
  else {
    /* jam: fileira de baixo (z..m); 'd' e 's' ficam para a navegação */
    int semi = ta_piano_semi(k);
    if (semi >= 0 && semi < 12)
      msm_jam(ta_player, 0, ta_octave * 12 + 12 + semi, ta_inst);
  }
  return 0;
}

static int ta_key_song(char k) {
  msm_header_t *h = &ta_song->header;
  int nitems = 3 + h->sequence_length;

  if (k == TA_KEY_UP || k == 'w')
    ta_song_sel = (ta_song_sel + nitems - 1) % nitems;
  else if (k == TA_KEY_DOWN || k == 's')
    ta_song_sel = (ta_song_sel + 1) % nitems;
  else if (k == TA_KEY_LEFT || k == TA_KEY_RIGHT || k == 'q' || k == 'd') {
    int d = (k == TA_KEY_RIGHT || k == 'd') ? 1 : -1;
    if (ta_song_sel == 0) {
      int v = h->speed + d;
      h->speed = (uint8_t)(v < 1 ? 1 : v > 31 ? 31 : v);
      msm_set_speed(ta_player, h->speed);
    } else if (ta_song_sel == 1) {
      int v = h->bpm + d * 5;
      h->bpm = (uint8_t)(v < 32 ? 32 : v > 255 ? 255 : v);
      msm_set_bpm(ta_player, h->bpm);
    } else if (ta_song_sel == 2) {
      int v = h->restart + d;
      if (v < 0) v = 0;
      if (v >= h->sequence_length) v = h->sequence_length - 1;
      h->restart = (uint8_t)v;
    } else {
      int idx = ta_song_sel - 3;
      int v = ta_song->sequence[idx] + d;
      if (v < 0) v = 0;
      if (v > h->pattern_count) v = h->pattern_count;  /* +1 cria pattern */
      if (v >= MSM_MAX_PATTERNS) v = MSM_MAX_PATTERNS - 1;
      ta_ensure_pattern(v);
      ta_song->sequence[idx] = (uint8_t)v;
    }
  }
  else if (k == 'i' && ta_song_sel >= 3 &&
           h->sequence_length < MSM_MAX_SEQUENCE) {
    int idx = ta_song_sel - 3;
    msm_edit_lock(ta_player);
    memmove(&ta_song->sequence[idx + 1], &ta_song->sequence[idx],
            (size_t)(h->sequence_length - idx));
    h->sequence_length++;
    msm_edit_unlock(ta_player);
  }
  else if ((k == 'x' || k == 8 || k == 127) && ta_song_sel >= 3 &&
           h->sequence_length > 1) {
    int idx = ta_song_sel - 3;
    msm_edit_lock(ta_player);
    memmove(&ta_song->sequence[idx], &ta_song->sequence[idx + 1],
            (size_t)(h->sequence_length - idx - 1));
    h->sequence_length--;
    if (h->restart >= h->sequence_length) h->restart = 0;
    msm_edit_unlock(ta_player);
    if (ta_song_sel - 3 >= h->sequence_length) ta_song_sel--;
  }
  else if ((k == '\n' || k == '\r' || k == 'a') && ta_song_sel >= 3) {
    ta_pos = ta_song_sel - 3;
    msm_play_from(ta_player, ta_pos, 0);
  }
  return 0;
}

static int ta_key_file(char k) {
  if (ta_saving) {
    size_t len = strlen(ta_savename);
    if (k == '\n' || k == '\r') {
      ta_saving = 0;
      if (len > 0) ta_save_song();
    } else if (k == TA_ESC) {
      ta_saving = 0;
    } else if ((k == 8 || k == 127) && len > 0) {
      ta_savename[len - 1] = '\0';
    } else if (len < sizeof(ta_savename) - 1 &&
               ((k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z') ||
                (k >= '0' && k <= '9') || k == '-' || k == '_')) {
      ta_savename[len] = k;
      ta_savename[len + 1] = '\0';
    }
    return 0;
  }

  int nitems = 1 + ta_file_count;
  if (k == TA_KEY_UP || k == 'w')
    ta_file_sel = (ta_file_sel + nitems - 1) % nitems;
  else if (k == TA_KEY_DOWN || k == 's')
    ta_file_sel = (ta_file_sel + 1) % nitems;
  else if (k == '\n' || k == '\r' || k == 'a') {
    if (ta_file_sel == 0) ta_saving = 1;
    else ta_load_song(ta_files[ta_file_sel - 1]);
  }
  return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Callbacks simplegfx
 * ══════════════════════════════════════════════════════════════════════════ */

void gfx_app(int init) {
  if (init >= 0) {
    ta_song = (msm_song_t *)calloc(1, sizeof(msm_song_t));
    if (!ta_song) return;
    ta_build_demo();
    ta_player = msm_create(ta_song);

    gfx_get_font_size(&ta_cw, &ta_chh, 1);
    ta_fs = (WINDOW_WIDTH / ta_cw >= 80) ? 2 : 1;
    gfx_get_font_size(&ta_cw, &ta_chh, ta_fs);
    ta_cols = WINDOW_WIDTH / ta_cw;
    ta_rows = WINDOW_HEIGHT / ta_chh;

    /* pluga o player num canal do engine; fica vivo até o fim do app */
    gfxa_stream(msm_fill, ta_player, NULL, GFXA_SAMPLE_RATE, GFXA_ASYNC);
    ta_message("ENTER toca - ESC menu");
  } else {
    if (ta_player) {
      msm_stop(ta_player);
      gfxa_stream_stop();
      gfx_delay(40);   /* deixa o fade-out do engine consumir o canal */
      msm_destroy(ta_player);
      ta_player = NULL;
    }
    if (ta_song) { free(ta_song); ta_song = NULL; }
  }
}

void gfx_process_data(int compute_time) { (void)compute_time; }

int gfx_draw(float fps) {
  (void)fps;
  if (!ta_song || !ta_player) return 1;

  gfx_set_color(12, 12, 24);
  gfx_fill_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

  switch (ta_screen) {
    case TA_SCR_PATTERN: ta_draw_pattern(); break;
    case TA_SCR_INST:    ta_draw_inst();    break;
    case TA_SCR_SONG:    ta_draw_song();    break;
    case TA_SCR_FILE:    ta_draw_file();    break;
    case TA_SCR_HELP:    ta_draw_help();    break;
  }

  if (ta_menu >= 0) ta_draw_menu();

  /* mensagem de status (sobrepõe o rodapé) */
  if (ta_msg_t > 0) {
    ta_msg_t--;
    gfx_set_color(20, 24, 38);
    ta_fill(0, ta_rows - 1, ta_cols, 1);
    gfx_set_color(250, 220, 120);
    ta_text(0, ta_rows - 1, ta_msg);
  } else if (ta_screen == TA_SCR_PATTERN) {
    gfx_set_color(90, 100, 120);
    ta_text(0, ta_rows - 1,
            ta_edit ? "piano=nota 1=off \\=cut .=apaga"
                    : "SPC=edita ENT=toca TAB=tela ESC=menu");
  }
  return 1;
}

/* Valores nas colunas inst/fx/param têm prioridade sobre os atalhos
 * globais (senão 'a' tocaria a música em vez de digitar hex A). */
static int ta_pattern_value_key(char k) {
  msm_cell_t *cell = ta_cell(ta_cur_row, ta_cur_ch);
  if (ta_cur_col == 1) {
    int v = ta_b32_digit(k);
    if (v >= 0 && v < MSM_MAX_INSTRUMENTS) {
      cell->instrument = (uint8_t)v;
      ta_inst = v;
      return 1;
    }
  } else if (ta_cur_col == 2) {
    int v = ta_hex_digit(k);
    if (v >= 0) { cell->effect = (uint8_t)v; return 1; }
  } else if (ta_cur_col >= 3) {
    int v = ta_hex_digit(k);
    if (v >= 0) {
      if (ta_cur_col == 3)
        cell->param = (uint8_t)((cell->param & 0x0F) | (v << 4));
      else
        cell->param = (uint8_t)((cell->param & 0xF0) | v);
      return 1;
    }
  }
  return 0;
}

int gfx_on_key(char key, int down) {
  if (!down || !ta_player) return 0;

  /* menu aberto consome tudo */
  if (ta_menu >= 0) return ta_key_menu(key);

  /* digitando nome de arquivo: o teclado inteiro é do campo */
  if (ta_screen == TA_SCR_FILE && ta_saving) return ta_key_file(key);

  /* digitando valores no pattern: hex/b32 antes dos atalhos */
  if (ta_screen == TA_SCR_PATTERN && ta_edit && ta_cur_col > 0 &&
      ta_pattern_value_key(key))
    return 0;

  /* globais */
  if (key == TA_ESC || (!ta_edit && key == 'u')) {  /* 'u' = MENU no rg35xx */
    if (ta_screen != TA_SCR_PATTERN) { ta_screen = TA_SCR_PATTERN; return 0; }
    ta_menu = 0;
    return 0;
  }
  if (key == TA_TAB) {
    ta_screen = (ta_screen + 1) % 5;
    if (ta_screen == TA_SCR_FILE) ta_scan_files();
    return 0;
  }
  if (key == '\n' || key == '\r' || key == 'a') {
    if (ta_screen == TA_SCR_PATTERN) { ta_toggle_play(); return 0; }
  }
  if (key == ' ') { ta_edit = !ta_edit; return 0; }
  if (key == 'l' && ta_screen == TA_SCR_PATTERN) {
    msm_set_loop_pattern(ta_player, !msm_get_loop_pattern(ta_player));
    ta_message(msm_get_loop_pattern(ta_player) ? "loop pattern ON"
                                               : "loop pattern OFF");
    return 0;
  }
  if (key == '[' && ta_screen != TA_SCR_INST) {
    if (ta_octave > 1) ta_octave--;
    return 0;
  }
  if (key == ']' && ta_screen != TA_SCR_INST) {
    if (ta_octave < 7) ta_octave++;
    return 0;
  }
  if ((key == '-' || key == '=') && ta_screen == TA_SCR_PATTERN) {
    int n = ta_song->header.instrument_count;
    if (n > 0) ta_inst = (ta_inst + (key == '-' ? n - 1 : 1)) % n;
    return 0;
  }
  if (key == '!' || key == '@' || key == '#' || key == '$') {
    int ch = key == '!' ? 0 : key == '@' ? 1 : key == '#' ? 2 : 3;
    msm_set_mute(ta_player, ch, !msm_get_mute(ta_player, ch));
    return 0;
  }

  switch (ta_screen) {
    case TA_SCR_PATTERN: return ta_key_pattern(key);
    case TA_SCR_INST:    return ta_key_inst(key);
    case TA_SCR_SONG:    return ta_key_song(key);
    case TA_SCR_FILE:    return ta_key_file(key);
    case TA_SCR_HELP:    ta_screen = TA_SCR_PATTERN; return 0;
  }
  return 0;
}
