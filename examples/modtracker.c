/* ══════════════════════════════════════════════════════════════════════════
 *  modtracker.c — exemplo de tracker modular sem samples (.MSM)
 *
 *  Compilar:
 *    make sdl-modtracker-run
 *
 *  Controles:
 *    ESPAÇO / ENTER  play/pause
 *    ← →              posição na sequência
 *    ↑ ↓              muda speed
 *    +/-              volume global
 *    0-9              salta para posição
 *    r                restart
 *    TAB              sair
 * ══════════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "simplegfx.h"
#include "audio_engine.h"
#include "sfxr.h"
#include "sfxr_presets.h"
#include "modtracker.h"

/* ─── Display ──────────────────────────────────────────────────────────── */

#define COL_BG      0x0F, 0x0B, 0x1A
#define COL_FRAME   0x2A, 0x2A, 0x4A
#define COL_NOTE    0x8F, 0xEE, 0x8F
#define COL_INST    0xEE, 0xDD, 0x66
#define COL_EFF     0x77, 0xBB, 0xEE
#define COL_ACTIVE  0xFF, 0xFF, 0x88
#define COL_TITLE   0x44, 0xFF, 0xCC
#define COL_POS     0xFF, 0x99, 0x55
#define COL_ROW_CUR 0x33, 0x44, 0x66

/* ─── Instrumentos (parametros sfxr float, convertidos na inicializacao) ── */

#define INST_COUNT 4

static const float inst_data[INST_COUNT][GFXA_SFXR_PARAM_COUNT] = {
  /* 0: Baixo — square limpo, punch medio, pitch-drop suave */
  {
    [GFXA_SFXR_WAVE_TYPE]     = 0,       /* square */
    [GFXA_SFXR_ATTACK]        = 0.01f,
    [GFXA_SFXR_SUSTAIN_TIME]  = 0.25f,
    [GFXA_SFXR_SUSTAIN_PUNCH] = 0.40f,
    [GFXA_SFXR_DECAY]         = 0.30f,
    [GFXA_SFXR_BASE_FREQ]     = 0.20f,
    [GFXA_SFXR_FREQ_LIMIT]    = 0.00f,
    [GFXA_SFXR_FREQ_RAMP]     = 0.55f,  /* pitch drop (norm: 0.55→0.1→pmul=0.999) */
    [GFXA_SFXR_FREQ_DRAMP]    = 0.50f,
    [GFXA_SFXR_VIB_STRENGTH]  = 0.00f,
    [GFXA_SFXR_VIB_SPEED]     = 0.00f,
    [GFXA_SFXR_ARP_MOD]       = 0.50f,
    [GFXA_SFXR_ARP_SPEED]     = 0.00f,
    [GFXA_SFXR_DUTY]          = 0.00f,
    [GFXA_SFXR_DUTY_RAMP]     = 0.50f,
    [GFXA_SFXR_REPEAT_SPEED]  = 0.00f,
    [GFXA_SFXR_PHA_OFFSET]    = 0.50f,
    [GFXA_SFXR_PHA_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_FREQ]      = 1.00f,
    [GFXA_SFXR_LPF_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_RESONANCE] = 0.00f,
    [GFXA_SFXR_HPF_FREQ]      = 0.00f,
    [GFXA_SFXR_HPF_RAMP]      = 0.50f,
    [GFXA_SFXR_SOUND_VOL]     = 0.50f,
  },

  /* 1: Lead — sawtooth, atk medio, sustain longo, vibrato embutido */
  {
    [GFXA_SFXR_WAVE_TYPE]     = 1,       /* sawtooth */
    [GFXA_SFXR_ATTACK]        = 0.08f,
    [GFXA_SFXR_SUSTAIN_TIME]  = 0.70f,
    [GFXA_SFXR_SUSTAIN_PUNCH] = 0.20f,
    [GFXA_SFXR_DECAY]         = 0.25f,
    [GFXA_SFXR_BASE_FREQ]     = 0.30f,
    [GFXA_SFXR_FREQ_LIMIT]    = 0.00f,
    [GFXA_SFXR_FREQ_RAMP]     = 0.00f,
    [GFXA_SFXR_FREQ_DRAMP]    = 0.50f,
    [GFXA_SFXR_VIB_STRENGTH]  = 0.30f,  /* vibrato suave */
    [GFXA_SFXR_VIB_SPEED]     = 0.40f,
    [GFXA_SFXR_ARP_MOD]       = 0.50f,
    [GFXA_SFXR_ARP_SPEED]     = 0.00f,
    [GFXA_SFXR_DUTY]          = 0.00f,
    [GFXA_SFXR_DUTY_RAMP]     = 0.50f,
    [GFXA_SFXR_REPEAT_SPEED]  = 0.00f,
    [GFXA_SFXR_PHA_OFFSET]    = 0.50f,
    [GFXA_SFXR_PHA_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_FREQ]      = 1.00f,
    [GFXA_SFXR_LPF_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_RESONANCE] = 0.00f,
    [GFXA_SFXR_HPF_FREQ]      = 0.00f,
    [GFXA_SFXR_HPF_RAMP]      = 0.50f,
    [GFXA_SFXR_SOUND_VOL]     = 0.45f,
  },

  /* 2: Arpejo — quadrado fino, 25% duty, sustain curto */
  {
    [GFXA_SFXR_WAVE_TYPE]     = 0,       /* square */
    [GFXA_SFXR_ATTACK]        = 0.01f,
    [GFXA_SFXR_SUSTAIN_TIME]  = 0.15f,
    [GFXA_SFXR_SUSTAIN_PUNCH] = 0.10f,
    [GFXA_SFXR_DECAY]         = 0.25f,
    [GFXA_SFXR_BASE_FREQ]     = 0.40f,
    [GFXA_SFXR_FREQ_LIMIT]    = 0.00f,
    [GFXA_SFXR_FREQ_RAMP]     = 0.00f,
    [GFXA_SFXR_FREQ_DRAMP]    = 0.50f,
    [GFXA_SFXR_VIB_STRENGTH]  = 0.00f,
    [GFXA_SFXR_VIB_SPEED]     = 0.00f,
    [GFXA_SFXR_ARP_MOD]       = 0.50f,
    [GFXA_SFXR_ARP_SPEED]     = 0.00f,
    [GFXA_SFXR_DUTY]          = 0.50f,   /* 25% duty (valor 0.5 no range [0,1]) */
    [GFXA_SFXR_DUTY_RAMP]     = 0.50f,
    [GFXA_SFXR_REPEAT_SPEED]  = 0.00f,
    [GFXA_SFXR_PHA_OFFSET]    = 0.50f,
    [GFXA_SFXR_PHA_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_FREQ]      = 1.00f,
    [GFXA_SFXR_LPF_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_RESONANCE] = 0.00f,
    [GFXA_SFXR_HPF_FREQ]      = 0.00f,
    [GFXA_SFXR_HPF_RAMP]      = 0.50f,
    [GFXA_SFXR_SOUND_VOL]     = 0.35f,
  },

  /* 3: Percussao — noise, 2 camadas: HH curto / caixa media */
  {
    [GFXA_SFXR_WAVE_TYPE]     = 3,       /* noise */
    [GFXA_SFXR_ATTACK]        = 0.001f,
    [GFXA_SFXR_SUSTAIN_TIME]  = 0.01f,
    [GFXA_SFXR_SUSTAIN_PUNCH] = 0.00f,
    [GFXA_SFXR_DECAY]         = 0.12f,
    [GFXA_SFXR_BASE_FREQ]     = 0.50f,
    [GFXA_SFXR_FREQ_LIMIT]    = 0.00f,
    [GFXA_SFXR_FREQ_RAMP]     = 0.00f,
    [GFXA_SFXR_FREQ_DRAMP]    = 0.50f,
    [GFXA_SFXR_VIB_STRENGTH]  = 0.00f,
    [GFXA_SFXR_VIB_SPEED]     = 0.00f,
    [GFXA_SFXR_ARP_MOD]       = 0.50f,
    [GFXA_SFXR_ARP_SPEED]     = 0.00f,
    [GFXA_SFXR_DUTY]          = 0.00f,
    [GFXA_SFXR_DUTY_RAMP]     = 0.50f,
    [GFXA_SFXR_REPEAT_SPEED]  = 0.00f,
    [GFXA_SFXR_PHA_OFFSET]    = 0.50f,
    [GFXA_SFXR_PHA_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_FREQ]      = 0.40f,  /* noise filtrado (snare-like) */
    [GFXA_SFXR_LPF_RAMP]      = 0.50f,
    [GFXA_SFXR_LPF_RESONANCE] = 0.10f,
    [GFXA_SFXR_HPF_FREQ]      = 0.15f,  /* corta grave */
    [GFXA_SFXR_HPF_RAMP]      = 0.50f,
    [GFXA_SFXR_SOUND_VOL]     = 0.50f,
  },
};

static const char *inst_names[INST_COUNT] = {
  "Bass", "Lead", "Arp", "Perc"
};

/* Helper: cria celula rapidamente — macro produz initializer compilavel */

#define C_(note, inst, fx, param) \
  { (uint8_t)((note) < 0 ? MSM_NOTE_NONE : (note)), \
    (uint8_t)((inst) < 0 ? 0xFF : (inst)), \
    (uint8_t)(fx), \
    (uint8_t)(param) }

/* Sem nota, sem inst */
#define E(fx, param)       C_(-1, -1, fx, param)
/* Nota + inst, sem efeito */
#define NI(note, inst)     C_(note, inst, 0, 0)
/* Nota + inst + efeito */
#define NIE(note, inst, fx, param) C_(note, inst, fx, param)
/* Silêncio */
#define REST               C_(-1, -1, 0, 0)
/* Note cut */
#define CUT                C_(-2, -1, 0, 0)

/* ─── Notas (MIDI) ─────────────────────────────────────────────────────── */

#define C2  36  /* MIDI note numbers */
#define Cs2 37
#define D2  38
#define Ds2 39
#define E2  40
#define F2  41
#define Fs2 42
#define G2  43
#define Gs2 44
#define A2  45
#define As2 46
#define B2  47
#define C3  48
#define Cs3 49
#define D3  50
#define Ds3 51
#define E3  52
#define F3  53
#define Fs3 54
#define G3  55
#define Gs3 56
#define A3  57
#define As3 58
#define B3  59
#define C4  60
#define Cs4 61
#define D4  62
#define Ds4 63
#define E4  64
#define F4  65
#define Fs4 66
#define G4  67
#define Gs4 68
#define A4  69
#define As4 70
#define B4  71
#define C5  72
#define Cs5 73
#define D5  74
#define Ds5 75
#define E5  76
#define F5  77
#define Fs5 78
#define G5  79
#define Gs5 80
#define A5  81
#define As5 82
#define B5  83
#define C6  84

/* ══════════════════════════════════════════════════════════════════════════
 *  Pattern definitions
 *
 *  4 canais:
 *    CH 0 = Baixo (inst 0)
 *    CH 1 = Lead  (inst 1)
 *    CH 2 = Arp   (inst 2)  — com arpejo 0x37 (tríade Am: 0, +3, +7)
 *    CH 3 = Perc  (inst 3)  — C5=HH, G4=snare
 *
 *  Escala: Am  (A B C D E F G)
 *  16 rows por pattern (rows 0-15)
 * ══════════════════════════════════════════════════════════════════════════ */

#define ROW(R, c0, c1, c2, c3) [R] = {c0, c1, c2, c3}

/* ─── Pattern 0: Intro — bass + perc (teste basico) ─────────────────── */

static msm_cell_t pat0[MSM_ROWS][MSM_CHANNELS] = {
  ROW( 0, NI(A2,0),          REST, REST, NI(C5,3) ),
  ROW( 1, REST,              REST, REST, REST     ),
  ROW( 2, NI(E2,0),          REST, REST, REST     ),
  ROW( 3, REST,              REST, REST, REST     ),
  ROW( 4, NI(F2,0),          REST, REST, NI(G4,3) ),
  ROW( 5, REST,              REST, REST, REST     ),
  ROW( 6, NI(G2,0),          REST, REST, REST     ),
  ROW( 7, REST,              REST, REST, REST     ),
  ROW( 8, NI(A2,0),          REST, REST, NI(C5,3) ),
  ROW( 9, REST,              REST, REST, REST     ),
  ROW(10, NI(E3,0),          REST, REST, REST     ),
  ROW(11, REST,              REST, REST, REST     ),
  ROW(12, NI(F3,0),          REST, REST, NI(G4,3) ),
  ROW(13, REST,              REST, REST, REST     ),
  ROW(14, NI(G3,0),          REST, REST, REST     ),
  ROW(15, REST,              REST, REST, REST     ),
};

/* ─── Pattern 1: Verse — bass + arp(0xy=Am arpejo) + perc ──────────── */

static msm_cell_t pat1[MSM_ROWS][MSM_CHANNELS] = {
  ROW( 0, NI(A2,0),  REST, NIE(A4,2,0x0,0x37), NI(C5,3) ),
  ROW( 1, REST,      REST, REST,                 REST     ),
  ROW( 2, NI(E2,0),  REST, NIE(E4,2,0x0,0x37), REST     ),
  ROW( 3, REST,      REST, REST,                 REST     ),
  ROW( 4, NI(F2,0),  REST, NIE(F4,2,0x0,0x37), NI(G4,3) ),
  ROW( 5, REST,      REST, REST,                 REST     ),
  ROW( 6, NI(G2,0),  REST, NIE(G4,2,0x0,0x37), REST     ),
  ROW( 7, REST,      REST, REST,                 REST     ),
  ROW( 8, NI(A2,0),  REST, NIE(A4,2,0x0,0x37), NI(C5,3) ),
  ROW( 9, REST,      REST, REST,                 REST     ),
  ROW(10, NI(E3,0),  REST, NIE(E5,2,0x0,0x37), REST     ),
  ROW(11, REST,      REST, REST,                 REST     ),
  ROW(12, NI(F3,0),  REST, NIE(F5,2,0x0,0x37), NI(G4,3) ),
  ROW(13, REST,      REST, REST,                 REST     ),
  ROW(14, NI(G3,0),  REST, NIE(G5,2,0x0,0x37), REST     ),
  ROW(15, REST,      REST, REST,                 REST     ),
};

/* ─── Pattern 2: Chorus — bass + lead(4xy vibrato) + arp + perc ────── */

static msm_cell_t pat2[MSM_ROWS][MSM_CHANNELS] = {
  ROW( 0, NI(A2,0),  NIE(A4,1,0x4,0x34), NIE(A4,2,0x0,0x37), NI(C5,3) ),
  ROW( 1, REST,      REST,                REST,                 REST     ),
  ROW( 2, NI(E2,0),  NIE(G4,1,0x4,0x23), NIE(E4,2,0x0,0x37), REST     ),
  ROW( 3, REST,      REST,                REST,                 REST     ),
  ROW( 4, NI(F2,0),  NIE(E4,1,0x4,0x34), NIE(F4,2,0x0,0x37), NI(G4,3) ),
  ROW( 5, REST,      REST,                REST,                 REST     ),
  ROW( 6, NI(G2,0),  NIE(F4,1,0x4,0x23), NIE(G4,2,0x0,0x37), REST     ),
  ROW( 7, REST,      REST,                REST,                 REST     ),
  ROW( 8, NI(A2,0),  NIE(G4,1,0x4,0x34), NIE(A4,2,0x0,0x37), NI(C5,3) ),
  ROW( 9, REST,      REST,                REST,                 REST     ),
  ROW(10, NI(E3,0),  NIE(C5,1,0x4,0x45), NIE(E5,2,0x0,0x37), REST     ),
  ROW(11, REST,      REST,                REST,                 REST     ),
  ROW(12, NI(F3,0),  NIE(B4,1,0x4,0x34), NIE(F5,2,0x0,0x37), NI(G4,3) ),
  ROW(13, REST,      REST,                REST,                 REST     ),
  ROW(14, NI(G3,0),  NIE(C5,1,0x4,0x45), NIE(G5,2,0x0,0x37), REST     ),
  ROW(15, REST,      REST,                REST,                 REST     ),
};

/* ─── Pattern 3: Bridge — bass portamento + lead volume slide + perc ─ */

static msm_cell_t pat3[MSM_ROWS][MSM_CHANNELS] = {
  /* Bass com portamento down (2xx) — slide de periodo */
  ROW( 0, NIE(A2,0,0x2,0x3), E(0xC,48),    REST, NI(C5,3) ),
  ROW( 1, REST,               REST,          REST, REST     ),
  ROW( 2, NIE(E2,0,0x2,0x4), REST,          REST, REST     ),
  ROW( 3, REST,               REST,          REST, REST     ),
  ROW( 4, NIE(F2,0,0x1,0x3), E(0xC,40),    REST, NI(G4,3) ),
  ROW( 5, REST,               REST,          REST, REST     ),
  ROW( 6, NIE(G2,0,0x1,0x4), REST,          REST, REST     ),
  ROW( 7, REST,               REST,          REST, REST     ),
  ROW( 8, NIE(A2,0,0x2,0x3), E(0xC,48),    REST, NI(C5,3) ),
  ROW( 9, REST,               REST,          REST, REST     ),
  ROW(10, NIE(E3,0,0x2,0x5), E(0xA,0x33),  REST, REST     ),
  ROW(11, REST,               REST,          REST, REST     ),
  ROW(12, NIE(F3,0,0x1,0x4), REST,       REST, NI(G4,3) ),
  ROW(13, REST,               REST,          REST, REST     ),
  ROW(14, NIE(G3,0,0x1,0x5), E(0xC,32),    REST, REST     ),
  ROW(15, REST,               REST,          REST, REST     ),
};

/* ─── Pattern 4: Outro — fade out, Bxx para loop ───────────────────── */

static msm_cell_t pat4[MSM_ROWS][MSM_CHANNELS] = {
  ROW( 0, NI(A2,0),  NIE(A4,1,0x4,0x23), NIE(A4,2,0x0,0x37), NI(C5,3) ),
  ROW( 1, REST,      REST,                REST,                 REST     ),
  ROW( 2, NI(E2,0),  REST,                REST,                 REST     ),
  ROW( 3, REST,      REST,                REST,                 REST     ),
  ROW( 4, NI(F2,0),  NIE(E4,1,0xC,0x28), REST,                 NI(G4,3) ),
  ROW( 5, REST,      REST,                REST,                 REST     ),
  ROW( 6, NI(G2,0),  REST,                REST,                 REST     ),
  ROW( 7, REST,      REST,                REST,                 REST     ),
  ROW( 8, REST,      E(0xC,16),           REST,                 NI(C5,3) ),
  ROW( 9, REST,      REST,                REST,                 REST     ),
  ROW(10, REST,      REST,                REST,                 REST     ),
  ROW(11, REST,      REST,                REST,                 REST     ),
  ROW(12, CUT,       CUT,                 CUT,                  CUT      ),
  ROW(13, REST,      REST,                REST,                 REST     ),
  /* Bxx = jump to position 0 (loopa a musica) */
  ROW(14, REST,      E(0xB,0),            REST,                 REST     ),
  ROW(15, REST,      REST,                REST,                 REST     ),
};

/* ══════════════════════════════════════════════════════════════════════════
 *  Montagem da música
 * ══════════════════════════════════════════════════════════════════════════ */

static msm_song_t song;

static void build_song(void) {
  memset(&song, 0, sizeof(song));

  /* Header */
  memset(&song.header, 0, sizeof(song.header));
  memcpy(song.header.magic, "MSM1", 4);
  snprintf(song.header.name, sizeof(song.header.name), "%s",
    "MSM Demo - Minha primeira musica sem samples");
  song.header.speed            = 6;
  song.header.pattern_count    = 5;
  song.header.instrument_count = INST_COUNT;
  song.header.sequence_length  = 12;
  song.header.channels         = 4;
  song.header.rows_per_pattern = 64;

  /* Sequence: Intro x2, Verse x2, Chorus x2, Bridge x2, Outro x3, Outro(loop) */
  int seq[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4};
  for (int i = 0; i < 12; i++)
    song.sequence[i] = (uint8_t)seq[i];

  /* Instrumentos */
  for (int i = 0; i < INST_COUNT; i++) {
    snprintf(song.instruments[i].name, sizeof(song.instruments[i].name), "%s", inst_names[i]);
    gfxa_sfxr_pack(inst_data[i], song.instruments[i].params);
  }

  /* Patterns */
  memcpy(&song.patterns[0], pat0, sizeof(pat0));
  memcpy(&song.patterns[1], pat1, sizeof(pat1));
  memcpy(&song.patterns[2], pat2, sizeof(pat2));
  memcpy(&song.patterns[3], pat3, sizeof(pat3));
  memcpy(&song.patterns[4], pat4, sizeof(pat4));
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Display helpers
 * ══════════════════════════════════════════════════════════════════════════ */

#define NOTE_NAMES "C-C#D-D#E-F-F#G-G#A-A#B-"
#define NOTE_NAMES_LEN 24

static const char *note_name(int midi) {
  static char buf[8];
  if (midi < 0) return "...";
  int oct = (midi / 12) - 1;
  int semitone = midi % 12;
  snprintf(buf, sizeof(buf), "%c%c%d",
           NOTE_NAMES[semitone * 2],
           NOTE_NAMES[semitone * 2 + 1] == '-' ? ' ' : '#',
           oct);
  return buf;
}

static const char *fx_name(int fx) {
  static const char *names[] = {
    "Arp","PrUp","PrDn","TPr","Vib","???","???","Tre",
    "Pan","???","VolS","Jmp","SetV","Brk","Ext","Spd"
  };
  if (fx < 0 || fx > 15) return "???";
  return names[fx];
}

/* ─── Estado global ───────────────────────────────────────────────────── */

static msm_player_t *player = NULL;
static int playing = 0;

static int dummy_fill(int16_t *buf, int n, void *user) {
  (void)buf; (void)n; (void)user;
  return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  App callbacks
 * ══════════════════════════════════════════════════════════════════════════ */

void gfx_app(int init) {
  if (init) {
    build_song();
    player = msm_create(&song);

    /* Mostra info carregada */
    printf("MSM Tracker Demo\n");
    printf("  Song: %s\n", song.header.name);
    printf("  Speed: %d\n", song.header.speed);
    printf("  Patterns: %d\n", song.header.pattern_count);
    printf("  Instruments: %d\n", song.header.instrument_count);
    printf("  Sequence length: %d\n", song.header.sequence_length);
    for (int i = 0; i < INST_COUNT; i++)
      printf("  Inst %d: %s\n", i, inst_names[i]);

    /* Bootstrap: chama gfxa_stream que no macOS/SDL cria a AudioQueue.
     * Um fill dummy que retorna 0 faz o engine alocar e parar
     * o canal imediatamente, mas a AudioQueue/thread fica ativa. */
    gfxa_stream(dummy_fill, NULL, NULL, GFXA_SAMPLE_RATE, GFXA_ASYNC);
    gfxa_engine_stop_all();

    /* Toca */
    msm_play(player);
    playing = 1;
    printf("Playing...\n");
  } else {
    if (player) {
      msm_stop(player);
      msm_destroy(player);
      player = NULL;
    }
    printf("MSM Tracker stopped.\n");
  }
}

void gfx_process_data(int ct) { (void)ct; }

/* ─── Render ──────────────────────────────────────────────────────────── */

static void draw_cell(int x, int y, const msm_cell_t *cell, int is_active,
                      int ch) {
  char buf[32];

  if (!cell || cell->note == MSM_NOTE_NONE) {
    /* Vazio */
    gfx_set_color(80, 80, 100);
    gfx_text("....", x, y, 1);
    return;
  }

  /* Note off / cut */
  if (cell->note == MSM_NOTE_CUT) {
    gfx_set_color(200, 60, 60);
    gfx_text("^^^", x, y, 1);
    return;
  }
  if (cell->note == MSM_NOTE_OFF) {
    gfx_set_color(180, 140, 60);
    gfx_text("===", x, y, 1);
    return;
  }

  /* Nota */
  if (is_active)
    gfx_set_color(COL_ACTIVE);
  else
    gfx_set_color(COL_NOTE);
  snprintf(buf, sizeof(buf), "%-3s", note_name(cell->note));
  gfx_text(buf, x, y, 1);

  /* Instrumento */
  if (cell->instrument < 32) {
    gfx_set_color(COL_INST);
    snprintf(buf, sizeof(buf), "%02d", cell->instrument);
    gfx_text(buf, x + 30, y, 1);
  }

  /* Efeito */
  if (cell->effect > 0 || cell->param > 0) {
    gfx_set_color(COL_EFF);
    snprintf(buf, sizeof(buf), "%s %02X", fx_name(cell->effect), cell->param);
    gfx_text(buf, x + 52, y, 1);
  }
}

int gfx_draw(float fps) {
  (void)fps;

  /* Tick do player */
  if (playing && player) {
    /* Roda ticks na taxa de frames (~60 Hz) */
    msm_tick(player);
  }

  /* ─── Fundo ──────────────────────────────────────────── */
  gfx_set_color(COL_BG);
  gfx_fill_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

  /* ─── Cabeçalho ──────────────────────────────────────── */
  gfx_set_color(COL_TITLE);
  gfx_text("MSM Tracker -- Synth Module (.msm)", 10, 8, 2);

  gfx_set_color(100, 120, 140);
  gfx_text("SPC play/pause  +/- vol  arrows nav  r restart  TAB quit", 10, 30, 1);

  char buf[128];
  snprintf(buf, sizeof(buf), "%s  |  Pos: %d/%d  Row: %02d  Tick: %d  Speed: %d  Vol: %.0f%%",
           song.header.name,
           player ? msm_get_position(player) : 0,
           msm_get_sequence_length(player) - 1,
           player ? msm_get_row(player) : 0,
           player ? msm_get_tick(player) : 0,
           player ? msm_get_speed(player) : song.header.speed,
           player ? msm_get_global_volume(player) * 100.0f : 100.0f);
  gfx_set_color(COL_POS);
  gfx_text(buf, 10, 46, 1);

  /* ─── Cabeçalho dos canais ───────────────────────────── */
  int x0 = 10;
  int y0 = 64;
  int row_h = 14;
  int col_w = 130;

  gfx_set_color(COL_FRAME);
  for (int ch = 0; ch < MSM_CHANNELS; ch++) {
    int x = x0 + ch * col_w;
    snprintf(buf, sizeof(buf), "── CH %d ──", ch);
    gfx_text(buf, x, y0, 1);
  }

  /* ─── Grid de patterns ───────────────────────────────── */
  int cur_row = player ? msm_get_row(player) : 0;
  int vis_rows = 14;
  int scroll = cur_row - vis_rows / 2;
  if (scroll < 0) scroll = 0;
  if (scroll > MSM_ROWS - vis_rows) scroll = MSM_ROWS - vis_rows;

  for (int r = 0; r < vis_rows && (r + scroll) < MSM_ROWS; r++) {
    int row = r + scroll;
    int y = y0 + 16 + r * row_h;

    /* Row highlight */
    if (row == cur_row && player) {
      gfx_set_color(COL_ROW_CUR);
      gfx_fill_rect(x0 - 2, y, col_w * MSM_CHANNELS + (MSM_CHANNELS-1)*20 + 4, row_h);
    }

    /* Row number */
    gfx_set_color(100, 90, 120);
    snprintf(buf, sizeof(buf), "%02X", row);
    gfx_text(buf, x0 - 20, y, 1);

    /* Cells */
    for (int ch = 0; ch < MSM_CHANNELS; ch++) {
      int x = x0 + ch * col_w;
      int pat = player ? msm_get_current_pattern(player) : 0;
      const msm_cell_t *cell = NULL;
      if (pat >= 0 && pat < song.header.pattern_count)
        cell = &song.patterns[pat].cell[row][ch];
      int active = (row == cur_row);
      draw_cell(x, y, cell, active, ch);
    }
  }

  /* ─── Info dos instrumentos ──────────────────────────── */
  y0 = y0 + 16 + vis_rows * row_h + 8;
  gfx_set_color(COL_FRAME);
  gfx_text("Instrumentos (sfxr):", 10, y0, 1);
  y0 += 16;

  for (int i = 0; i < INST_COUNT; i++) {
    gfx_set_color(180, 180, 180);
    snprintf(buf, sizeof(buf), "  %d: %s", i, inst_names[i]);
    gfx_text(buf, 10, y0 + i * 14, 1);

    /* Mostra waveform e info basica */
    const float *p = inst_data[i];
    const char *wave = "???";
    switch ((int)p[GFXA_SFXR_WAVE_TYPE]) {
      case 0: wave = "SQR"; break;
      case 1: wave = "SAW"; break;
      case 2: wave = "SIN"; break;
      case 3: wave = "NOI"; break;
    }
    gfx_set_color(140, 160, 180);
    snprintf(buf, sizeof(buf), "  wave=%s atk=%.2f sus=%.2f dec=%.2f",
             wave, p[GFXA_SFXR_ATTACK], p[GFXA_SFXR_SUSTAIN_TIME],
             p[GFXA_SFXR_DECAY]);
    gfx_text(buf, 110, y0 + i * 14, 1);
  }

  /* ─── Legenda de efeitos ─────────────────────────────── */
  y0 += INST_COUNT * 14 + 8;
  gfx_set_color(80, 100, 120);
  gfx_text("Efeitos: 0xy=Arp 1xx=PrUp 2xx=PrDn 3xx=TPort 4xy=Vib "
           "7xy=Trm Axy=VolS Cxx=Vol Dxx=Brk Fxx=Spd",
           10, y0, 1);

  return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Keyboard input
 * ══════════════════════════════════════════════════════════════════════════ */

int gfx_on_key(char key, int down) {
  if (!down) return 0;
  if (!player) return 1;

  switch (key) {
    case BTN_GP_A:
    case ' ':
    case '\n':
    case '\r':
      if (playing) {
        msm_stop(player);
        playing = 0;
      } else {
        msm_play(player);
        playing = 1;
      }
      return 0;

    case 'r':
    case 'R':
      msm_stop(player);
      msm_play(player);
      playing = 1;
      return 0;

    case BTN_GP_UP:
      msm_set_speed(player, msm_get_speed(player) + 1);
      return 0;

    case BTN_GP_DOWN:
      msm_set_speed(player, msm_get_speed(player) - 1);
      return 0;

    case BTN_GP_LEFT:
      msm_set_position(player, msm_get_position(player) - 1);
      return 0;

    case BTN_GP_RIGHT:
      msm_set_position(player, msm_get_position(player) + 1);
      return 0;

    case '+':
    case '=': {
      float v = msm_get_global_volume(player) + 0.1f;
      if (v > 1.0f) v = 1.0f;
      msm_set_global_volume(player, v);
      return 0;
    }

    case '-':
    case '_': {
      float v = msm_get_global_volume(player) - 0.1f;
      if (v < 0.0f) v = 0.0f;
      msm_set_global_volume(player, v);
      return 0;
    }

    case '0' ... '9': {
      int pos = key - '0';
      int max = msm_get_sequence_length(player) - 1;
      if (pos > max) pos = max;
      msm_set_position(player, pos);
      msm_set_row(player, 0);
      return 0;
    }

    case BTN_GP_MENU:
      return 1; /* quit */
  }

  return 0;
}
