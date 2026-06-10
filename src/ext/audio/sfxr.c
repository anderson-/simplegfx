#include "sfxr.h"
#include "simplegfx.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

/* Mapeia [0,1] → [-1,1] para parâmetros SIGNED */
static float norm_signed(float v) {
  return (v - 0.5f) * 2.0f;
}

static const char *skip_ws(const char *p) {
  while (*p && (unsigned char)*p <= ' ') ++p;
  return p;
}

static const char *parse_float(const char *p, float *out) {
  p = skip_ws(p);
  char *end = NULL;
  *out = (float)strtod(p, &end);
  return end ? end : p;
}

/* ── CSV parser ──────────────────────────────────────────────────────────── */

int gfxa_sfxr_parse(const char *str, float params[GFXA_SFXR_PARAM_COUNT]) {
  if (!str || !str[0]) return 1;
  gfxa_sfxr_defaults(params);
  const char *p = str;
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; ++i) {
    p = parse_float(p, &params[i]);
    if (*p == ',') ++p;
  }
  return 0;
}

/* ── Minimal JSON parser ─────────────────────────────────────────────────── */

/* Compara duas strings case-insensitive. Retorna true se forem iguais. */
static bool str_ieq(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    ++a; ++b;
  }
  return (*a == '\0' && *b == '\0');
}

/* Lê uma string JSON quotada (com ou sem escape).
 * Retorna o ponteiro após a quote de fechamento.
 * Se out_key não for NULL, copia o conteúdo para out_key (sem escapes, sem quotes).
 * max_key é o tamanho do buffer out_key.
 */
static const char *json_read_quoted(const char *p, char *out_key, int max_key) {
  if (*p != '"' && *p != '\'') return p;
  char q = *p++;
  int i = 0;
  while (*p && *p != q) {
    if (*p == '\\' && *(p+1)) {
      if (out_key && i < max_key-1) out_key[i++] = *(p+1);
      p += 2;
    } else {
      if (out_key && i < max_key-1) out_key[i++] = *p;
      ++p;
    }
  }
  if (out_key && i < max_key) out_key[i] = '\0';
  if (*p) ++p; /* skip closing quote */
  return p;
}

/* Pula um valor JSON (qualquer tipo: string, number, bool, null, objeto, array).
 * Retorna ponteiro após o valor.
 */
static const char *json_skip_value(const char *p) {
  p = skip_ws(p);
  if (!*p) return p;
  switch (*p) {
    case '"': case '\'': p = json_read_quoted(p, NULL, 0); break;
    case 't': case 'T': p += 4; break;
    case 'f': case 'F': p += 5; break;
    case 'n': case 'N': p += 4; break;
    case '{': case '[': {
      char c = (*p == '{') ? '}' : ']';
      int d = 1; ++p;
      while (*p && d > 0) {
        if (*p == c) --d;
        else if (*p == '{' || *p == '[') ++d;
        else if (*p == '"' || *p == '\'') p = json_read_quoted(p, NULL, 0) - 1;
        ++p;
      }
      break;
    }
    default:
      /* número ou palavra-chave */
      while (*p && *p != ',' && *p != '}' && *p != ']' && (unsigned char)*p > ' ') ++p;
      break;
  }
  return p;
}

/* Procura no JSON a chave `key` (case-insensitive) e retorna seu valor numérico.
 * Suporta chaves quotadas e não-quotadas.
 */
static bool json_read_float(const char *json, const char *key, float *out) {
  const char *p = json;
  char buf[64];
  while (*p) {
    p = skip_ws(p);
    if (*p == '}') break;
    if (*p == ',' || *p == '{') { ++p; continue; }
    p = skip_ws(p);
    if (!*p) break;
    
    /* Lê a chave */
    const char *after_key;
    bool key_matches = false;
    
    if (*p == '"' || *p == '\'') {
      /* Chave quotada — extrai e compara */
      after_key = json_read_quoted(p, buf, sizeof(buf));
      key_matches = str_ieq(buf, key);
    } else if ((unsigned char)*p > ' ') {
      /* Chave não-quotada */
      const char *kp = p, *k = key;
      while (*k && tolower((unsigned char)*kp) == tolower((unsigned char)*k)) { ++kp; ++k; }
      key_matches = (!*k && ((unsigned char)*kp <= ' ' || *kp == ':'));
      if (key_matches) after_key = kp;
      else {
        /* Pula o token não-quotado */
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ':' && (unsigned char)*p > ' ') ++p;
        after_key = p;
      }
    } else {
      break;
    }
    
    p = skip_ws(after_key);
    if (*p != ':') continue;
    ++p; p = skip_ws(p);
    
    if (!key_matches) {
      /* Chave não é a que procuramos — pula o valor */
      p = json_skip_value(p);
      continue;
    }
    
    /* Chave encontrada — lê o valor numérico */
    if (*p == '"' || *p == '\'') ++p;
    char *end = NULL;
    *out = (float)strtod(p, &end);
    if (end != p) return true;
    return false;
  }
  return false;
}

/* field-name → index mapping (todos os nomes do jsfxr + variações) */
static const struct { const char *name; int idx; } field_map[] = {
  /* wave */
  {"wave_type",      GFXA_SFXR_WAVE_TYPE},
  {"waveType",       GFXA_SFXR_WAVE_TYPE},
  /* envelope */
  {"p_env_attack",   GFXA_SFXR_ATTACK},
  {"env_attack",     GFXA_SFXR_ATTACK},
  {"envAttack",      GFXA_SFXR_ATTACK},
  {"attack",         GFXA_SFXR_ATTACK},
  {"p_env_sustain",  GFXA_SFXR_SUSTAIN_TIME},
  {"env_sustain",    GFXA_SFXR_SUSTAIN_TIME},
  {"envSustain",     GFXA_SFXR_SUSTAIN_TIME},
  {"sustain_time",   GFXA_SFXR_SUSTAIN_TIME},
  {"p_env_release",  GFXA_SFXR_SUSTAIN_TIME},
  {"env_release",    GFXA_SFXR_SUSTAIN_TIME},
  {"release",        GFXA_SFXR_SUSTAIN_TIME},
  {"p_env_punch",    GFXA_SFXR_SUSTAIN_PUNCH},
  {"env_punch",      GFXA_SFXR_SUSTAIN_PUNCH},
  {"sustain",        GFXA_SFXR_SUSTAIN_PUNCH},
  {"sustain_punch",  GFXA_SFXR_SUSTAIN_PUNCH},
  {"p_env_decay",    GFXA_SFXR_DECAY},
  {"env_decay",      GFXA_SFXR_DECAY},
  {"envDecay",       GFXA_SFXR_DECAY},
  {"decay",          GFXA_SFXR_DECAY},
  /* freq */
  {"p_base_freq",    GFXA_SFXR_BASE_FREQ},
  {"base_freq",      GFXA_SFXR_BASE_FREQ},
  {"baseFreq",       GFXA_SFXR_BASE_FREQ},
  {"freq",           GFXA_SFXR_BASE_FREQ},
  {"p_freq_limit",   GFXA_SFXR_FREQ_LIMIT},
  {"freq_limit",     GFXA_SFXR_FREQ_LIMIT},
  {"freqLimit",      GFXA_SFXR_FREQ_LIMIT},
  {"p_freq_ramp",    GFXA_SFXR_FREQ_RAMP},
  {"freq_ramp",      GFXA_SFXR_FREQ_RAMP},
  {"freqRamp",       GFXA_SFXR_FREQ_RAMP},
  {"p_freq_dramp",   GFXA_SFXR_FREQ_DRAMP},
  {"freq_dramp",     GFXA_SFXR_FREQ_DRAMP},
  {"freqDramp",      GFXA_SFXR_FREQ_DRAMP},
  {"freqDeltaRamp",  GFXA_SFXR_FREQ_DRAMP},
  /* vibrato */
  {"p_vib_strength", GFXA_SFXR_VIB_STRENGTH},
  {"vib_strength",   GFXA_SFXR_VIB_STRENGTH},
  {"vib_depth",      GFXA_SFXR_VIB_STRENGTH},
  {"vibDepth",       GFXA_SFXR_VIB_STRENGTH},
  {"p_vib_speed",    GFXA_SFXR_VIB_SPEED},
  {"vib_speed",      GFXA_SFXR_VIB_SPEED},
  {"vibSpeed",       GFXA_SFXR_VIB_SPEED},
  /* arpeggio (agora separados do repeat) */
  {"p_arp_mod",      GFXA_SFXR_ARP_MOD},
  {"arp_mod",        GFXA_SFXR_ARP_MOD},
  {"arpMod",         GFXA_SFXR_ARP_MOD},
  {"p_arp_speed",    GFXA_SFXR_ARP_SPEED},
  {"arp_speed",      GFXA_SFXR_ARP_SPEED},
  {"arpSpeed",       GFXA_SFXR_ARP_SPEED},
  /* duty */
  {"p_duty",         GFXA_SFXR_DUTY},
  {"duty",           GFXA_SFXR_DUTY},
  {"square_duty",    GFXA_SFXR_DUTY},
  {"squareDuty",     GFXA_SFXR_DUTY},
  {"p_duty_ramp",    GFXA_SFXR_DUTY_RAMP},
  {"duty_ramp",      GFXA_SFXR_DUTY_RAMP},
  {"duty_sweep",     GFXA_SFXR_DUTY_RAMP},
  {"dutySweep",      GFXA_SFXR_DUTY_RAMP},
  /* repeat (agora só controla repeat, não arpeggio) */
  {"p_repeat_speed", GFXA_SFXR_REPEAT_SPEED},
  {"repeat_speed",   GFXA_SFXR_REPEAT_SPEED},
  {"repeatSpeed",    GFXA_SFXR_REPEAT_SPEED},
  /* phaser */
  {"p_pha_offset",   GFXA_SFXR_PHA_OFFSET},
  {"pha_offset",     GFXA_SFXR_PHA_OFFSET},
  {"phaOffset",      GFXA_SFXR_PHA_OFFSET},
  {"phaser_offset",  GFXA_SFXR_PHA_OFFSET},
  {"phaserOffset",   GFXA_SFXR_PHA_OFFSET},
  {"p_pha_ramp",     GFXA_SFXR_PHA_RAMP},
  {"pha_ramp",       GFXA_SFXR_PHA_RAMP},
  {"phaRamp",        GFXA_SFXR_PHA_RAMP},
  {"phaser_sweep",   GFXA_SFXR_PHA_RAMP},
  {"phaserSweep",    GFXA_SFXR_PHA_RAMP},
  {"pha_sweep",      GFXA_SFXR_PHA_RAMP},
  {"phaSweep",       GFXA_SFXR_PHA_RAMP},
  /* LPF */
  {"p_lpf_freq",     GFXA_SFXR_LPF_FREQ},
  {"lpf_freq",       GFXA_SFXR_LPF_FREQ},
  {"lpfFreq",        GFXA_SFXR_LPF_FREQ},
  {"lpf_cutoff",     GFXA_SFXR_LPF_FREQ},
  {"lpfCutoff",      GFXA_SFXR_LPF_FREQ},
  {"p_lpf_ramp",     GFXA_SFXR_LPF_RAMP},
  {"lpf_ramp",       GFXA_SFXR_LPF_RAMP},
  {"lpfRamp",        GFXA_SFXR_LPF_RAMP},
  {"p_lpf_resonance",GFXA_SFXR_LPF_RESONANCE},
  {"lpf_resonance",  GFXA_SFXR_LPF_RESONANCE},
  {"lpfResonance",   GFXA_SFXR_LPF_RESONANCE},
  /* HPF */
  {"p_hpf_freq",     GFXA_SFXR_HPF_FREQ},
  {"hpf_freq",       GFXA_SFXR_HPF_FREQ},
  {"hpfFreq",        GFXA_SFXR_HPF_FREQ},
  {"hpf_cutoff",     GFXA_SFXR_HPF_FREQ},
  {"hpfCutoff",      GFXA_SFXR_HPF_FREQ},
  {"p_hpf_ramp",     GFXA_SFXR_HPF_RAMP},
  {"hpf_ramp",       GFXA_SFXR_HPF_RAMP},
  {"hpfRamp",        GFXA_SFXR_HPF_RAMP},
  {/* sound_vol (não está no params_order jsfxr mas é usado internamente) */
   "sound_vol",      GFXA_SFXR_SOUND_VOL},
  {"soundVol",       GFXA_SFXR_SOUND_VOL},
  {NULL, 0}
};

/* Parâmetros SIGNED — valores no JSON jsfxr estão em [-1, 1] (0 = centro)  */
/* Precisam ser convertidos para [0, 1] (0.5 = centro) antes de armazenar */

static bool is_signed_param(int idx) {
  switch (idx) {
    case GFXA_SFXR_FREQ_RAMP:
    case GFXA_SFXR_FREQ_DRAMP:
    case GFXA_SFXR_ARP_MOD:
    case GFXA_SFXR_DUTY_RAMP:
    case GFXA_SFXR_PHA_OFFSET:
    case GFXA_SFXR_PHA_RAMP:
    case GFXA_SFXR_LPF_RAMP:
    case GFXA_SFXR_HPF_RAMP:
      return true;
    default:
      return false;
  }
}

int gfxa_sfxr_parse_json(const char *json, float params[GFXA_SFXR_PARAM_COUNT]) {
  if (!json || !json[0]) return 1;
  gfxa_sfxr_defaults(params);
  for (int i = 0; field_map[i].name; ++i) {
    float v;
    if (json_read_float(json, field_map[i].name, &v)) {
      /* signed params no JSON estão em [-1,1]; converter para [0,1] */
      if (is_signed_param(field_map[i].idx))
        v = (v + 1.0f) / 2.0f;
      params[field_map[i].idx] = clampf(v, 0.0f, 1.0f);
    }
  }
  return 0;
}

/* ── Defaults (idêntico ao construtor Parms() do jsfxr) ────────────────── */

void gfxa_sfxr_defaults(float params[GFXA_SFXR_PARAM_COUNT]) {
  params[GFXA_SFXR_WAVE_TYPE]      = 0.0f;   /* SQUARE */
  params[GFXA_SFXR_ATTACK]         = 0.0f;   /* p_env_attack = 0 */
  params[GFXA_SFXR_SUSTAIN_TIME]   = 0.3f;   /* p_env_sustain = 0.3 */
  params[GFXA_SFXR_SUSTAIN_PUNCH]  = 0.0f;   /* p_env_punch = 0 */
  params[GFXA_SFXR_DECAY]          = 0.4f;   /* p_env_decay = 0.4 */
  params[GFXA_SFXR_BASE_FREQ]      = 0.3f;   /* p_base_freq = 0.3 */
  params[GFXA_SFXR_FREQ_LIMIT]     = 0.0f;   /* p_freq_limit = 0 */
  params[GFXA_SFXR_FREQ_RAMP]      = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_FREQ_DRAMP]     = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_VIB_STRENGTH]   = 0.0f;   /* p_vib_strength = 0 */
  params[GFXA_SFXR_VIB_SPEED]      = 0.0f;   /* p_vib_speed = 0 */
  params[GFXA_SFXR_ARP_MOD]        = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_ARP_SPEED]      = 0.0f;   /* p_arp_speed = 0 */
  params[GFXA_SFXR_DUTY]           = 0.0f;   /* p_duty = 0 */
  params[GFXA_SFXR_DUTY_RAMP]      = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_REPEAT_SPEED]   = 0.0f;   /* p_repeat_speed = 0 */
  params[GFXA_SFXR_PHA_OFFSET]     = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_PHA_RAMP]       = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_LPF_FREQ]       = 1.0f;   /* p_lpf_freq = 1 */
  params[GFXA_SFXR_LPF_RAMP]       = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_LPF_RESONANCE]  = 0.0f;   /* p_lpf_resonance = 0 */
  params[GFXA_SFXR_HPF_FREQ]       = 0.0f;   /* p_hpf_freq = 0 */
  params[GFXA_SFXR_HPF_RAMP]       = 0.5f;   /* 0 (signed) */
  params[GFXA_SFXR_SOUND_VOL]      = 0.5f;   /* sound_vol = 0.5 */
}

/* ── Síntese — implementação fiel ao jsfxr SoundEffect.getRawBuffer ────── */

int gfxa_sfxr_generate(const float params[GFXA_SFXR_PARAM_COUNT],
                        int16_t *samples, int max_samples, int sample_rate) {
  if (!samples || max_samples < 64 || sample_rate < 8000) return 0;

  /* ── Extrai e converte todos os parâmetros ───────────────────────────── */
  int wt = (int)clampf(params[GFXA_SFXR_WAVE_TYPE], 0, 3);
  float pa  = clampf(params[GFXA_SFXR_ATTACK], 0, 1);        /* p_env_attack */
  float pst = clampf(params[GFXA_SFXR_SUSTAIN_TIME], 0, 1);  /* p_env_sustain (duration) */
  float ppu = clampf(params[GFXA_SFXR_SUSTAIN_PUNCH], 0, 1); /* p_env_punch (level) */
  float pd  = clampf(params[GFXA_SFXR_DECAY], 0, 1);         /* p_env_decay */
  float pbf = clampf(params[GFXA_SFXR_BASE_FREQ], 0, 1);
  float pfl = clampf(params[GFXA_SFXR_FREQ_LIMIT], 0, 1);
  float pfr = norm_signed(clampf(params[GFXA_SFXR_FREQ_RAMP], 0, 1));
  float pfd = norm_signed(clampf(params[GFXA_SFXR_FREQ_DRAMP], 0, 1));
  float pvstrength = clampf(params[GFXA_SFXR_VIB_STRENGTH], 0, 1);  /* p_vib_strength */
  float pvspeed    = clampf(params[GFXA_SFXR_VIB_SPEED], 0, 1);     /* p_vib_speed */
  float parp_mod = norm_signed(clampf(params[GFXA_SFXR_ARP_MOD], 0, 1));
  float parp_spd = clampf(params[GFXA_SFXR_ARP_SPEED], 0, 1);
  float pdu = clampf(params[GFXA_SFXR_DUTY], 0, 1);
  float pds = norm_signed(clampf(params[GFXA_SFXR_DUTY_RAMP], 0, 1));
  float prp = clampf(params[GFXA_SFXR_REPEAT_SPEED], 0, 1);
  float pho = norm_signed(clampf(params[GFXA_SFXR_PHA_OFFSET], 0, 1));
  float phs = norm_signed(clampf(params[GFXA_SFXR_PHA_RAMP], 0, 1));
  float plc = clampf(params[GFXA_SFXR_LPF_FREQ], 0, 1);
  float plrm = norm_signed(clampf(params[GFXA_SFXR_LPF_RAMP], 0, 1));
  float plr = clampf(params[GFXA_SFXR_LPF_RESONANCE], 0, 1);
  float phc = clampf(params[GFXA_SFXR_HPF_FREQ], 0, 1);
  float phrm = norm_signed(clampf(params[GFXA_SFXR_HPF_RAMP], 0, 1));
  float psndvol = clampf(params[GFXA_SFXR_SOUND_VOL], 0, 1);

  /* ── init() — valores computados uma vez ─────────────────────────────── */

  /* Wave shape */
  int wave_shape = wt;

  /* LPF (state variable) */
  float fltw = plc * plc * plc * 0.1f;
  int en_lpf = (plc != 1.0f);
  float fltw_d = 1.0f + plrm * 0.0001f;
  float fltdmp = 5.0f / (1.0f + plr * plr * 20.0f) * (0.01f + fltw);
  if (fltdmp > 0.8f) fltdmp = 0.8f;
  float fltp = 0.0f, fltdp = 0.0f;

  /* HPF (state variable) */
  float flthp = phc * phc * 0.1f;
  float flthp_d = 1.0f + phrm * 0.0003f;
  float fltphp = 0.0f;

  /* Vibrato */
  float vib_speed = pvspeed * pvspeed * 0.01f;
  float vib_amp   = pvstrength * 0.5f;
  float vib_phase = 0.0f;

  /* Envelope lengths */
  int env_len[3] = {
    (int)(pa * pa * 100000.0f),
    (int)(pst * pst * 100000.0f),
    (int)(pd * pd * 100000.0f)
  };
  int env_stage = 0, env_elapsed = 0;

  /* Flanger */
  float flg_buf[1024];
  memset(flg_buf, 0, sizeof(flg_buf));
  float flg_off = (pho >= 0.0f ? 1.0f : -1.0f) * (pho * pho) * 1020.0f;
  float flg_sl  = (phs >= 0.0f ? 1.0f : -1.0f) * (phs * phs);
  int flg_pos = 0;

  /* Repeat time */
  int rpt_time = (int)((1.0f - prp) * (1.0f - prp) * 20000.0f + 32.0f);
  if (prp <= 0.0f) rpt_time = 0;

  /* Gain = exp(sound_vol) - 1 */
  float gain = expf(psndvol) - 1.0f;

  /* ── initForRepeat() — valores que resetam no repeat ────────────────── */

  float period = 100.0f / (pbf * pbf + 0.001f);
  float period_max = 100.0f / (pfl * pfl + 0.001f);
  int freq_cut = (pfl > 0.0f);
  float pmul = 1.0f - (pfr * pfr * pfr) * 0.01f;
  float pslide = -(pfd * pfd * pfd) * 0.000001f;
  float duty = 0.5f - pdu * 0.5f;
  float dslide = -pds * 0.00005f;
  float arp_mult;
  if (parp_mod >= 0.0f)
    arp_mult = 1.0f - (parp_mod * parp_mod) * 0.9f;
  else
    arp_mult = 1.0f + (parp_mod * parp_mod) * 10.0f;
  int arp_time = (int)((1.0f - parp_spd) * (1.0f - parp_spd) * 20000.0f + 32.0f);
  if (parp_spd >= 1.0f) arp_time = 0;
  int rpt_elapsed = 0;

  /* Noise buffer (32 amostras) */
  float noise[32];
  for (int i = 0; i < 32; i++)
    noise[i] = (float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f;

  /* Downsampling: 44100 → sample_rate */
  int summands = 44100 / sample_rate;
  if (summands < 1) summands = 1;
  int summed = 0;
  float accum = 0.0f;

  /* ── Loop principal de síntese ──────────────────────────────────────── */
  int phase = 0, out = 0;

  for (int t = 0; out < max_samples; t++) {

    /* ── Repeat ──────────────────────────────────────────────────────── */
    if (rpt_time != 0) {
      rpt_elapsed++;
      if (rpt_elapsed >= rpt_time) {
        rpt_elapsed = 0;
        /* initForRepeat() */
        period = 100.0f / (pbf * pbf + 0.001f);
        period_max = 100.0f / (pfl * pfl + 0.001f);
        freq_cut = (pfl > 0.0f);
        pmul = 1.0f - (pfr * pfr * pfr) * 0.01f;
        pslide = -(pfd * pfd * pfd) * 0.000001f;
        duty = 0.5f - pdu * 0.5f;
        dslide = -pds * 0.00005f;
        if (parp_mod >= 0.0f)
          arp_mult = 1.0f - (parp_mod * parp_mod) * 0.9f;
        else
          arp_mult = 1.0f + (parp_mod * parp_mod) * 10.0f;
        arp_time = (int)((1.0f - parp_spd) * (1.0f - parp_spd) * 20000.0f + 32.0f);
        if (parp_spd >= 1.0f) arp_time = 0;
      }
    }

    /* ── Arpeggio (one-shot) ─────────────────────────────────────────── */
    if (arp_time != 0 && t >= arp_time) {
      arp_time = 0;
      period *= arp_mult;
    }

    /* ── Frequency slide ─────────────────────────────────────────────── */
    pmul += pslide;
    period *= pmul;
    if (period > period_max) {
      period = period_max;
      if (freq_cut) break;
    }

    /* ── Vibrato ─────────────────────────────────────────────────────── */
    float rfperiod = period;
    if (vib_amp > 0.0f) {
      vib_phase += vib_speed;
      rfperiod = period * (1.0f + sinf(vib_phase) * vib_amp);
    }
    int iperiod = (int)rfperiod;
    if (iperiod < 8) iperiod = 8;

    /* ── Duty slide ──────────────────────────────────────────────────── */
    duty += dslide;
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 0.5f) duty = 0.5f;

    /* ── Envelope ────────────────────────────────────────────────────── */
    if (++env_elapsed > env_len[env_stage]) {
      env_elapsed = 0;
      if (++env_stage > 2) break;
    }
    float envf = env_len[env_stage] > 0
                   ? (float)env_elapsed / env_len[env_stage] : 0.0f;
    float ev;
    if (env_stage == 0)
      ev = envf;
    else if (env_stage == 1)
      ev = 1.0f + (1.0f - envf) * 2.0f * ppu;
    else
      ev = 1.0f - envf;

    /* ── Flanger offset slide ────────────────────────────────────────── */
    flg_off += flg_sl;
    int iphase = (int)fabsf(floorf(flg_off));
    if (iphase > 1023) iphase = 1023;

    /* ── HPF slide ───────────────────────────────────────────────────── */
    flthp *= flthp_d;
    if (flthp < 0.00001f) flthp = 0.00001f;
    if (flthp > 0.1f) flthp = 0.1f;

    /* ── 8× oversampling ────────────────────────────────────────────── */
    float sample = 0.0f;
    for (int si = 0; si < 8; si++) {
      float ss = 0.0f;
      phase++;
      if (phase >= iperiod) {
        phase %= iperiod;
        if (wave_shape == GFXA_SFXR_WAVE_NOISE) {
          for (int j = 0; j < 32; j++)
            noise[j] = (float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f;
        }
      }

      float fp = (float)phase / iperiod;

      /* Gera forma de onda base */
      switch (wave_shape) {
        case 0: /* SQUARE */
          ss = (fp < duty) ? 0.5f : -0.5f;
          break;
        case 1: /* SAWTOOTH */
          if (fp < duty)
            ss = -1.0f + 2.0f * fp / duty;
          else
            ss = 1.0f - 2.0f * (fp - duty) / (1.0f - duty);
          break;
        case 2: /* SINE */
          ss = sinf(fp * 2.0f * (float)M_PI);
          break;
        case 3: /* NOISE */
          ss = noise[(phase * 32 / iperiod) % 32];
          break;
      }

      /* ── Low-pass filter (state variable) ──────────────────────────── */
      float pp = fltp;
      fltw *= fltw_d;
      if (fltw < 0.0f) fltw = 0.0f;
      if (fltw > 0.1f) fltw = 0.1f;
      if (en_lpf) {
        fltdp += (ss - fltp) * fltw;
        fltdp -= fltdp * fltdmp;
      } else {
        fltp = ss;
        fltdp = 0.0f;
      }
      fltp += fltdp;

      /* ── High-pass filter ──────────────────────────────────────────── */
      fltphp += fltp - pp;
      fltphp -= fltphp * flthp;
      ss = fltphp;

      /* ── Flanger ───────────────────────────────────────────────────── */
      flg_buf[flg_pos & 1023] = ss;
      ss += flg_buf[(flg_pos - iphase + 1024) & 1023];
      flg_pos = (flg_pos + 1) & 1023;

      sample += ss * ev;
    }

    /* ── Downsampling ────────────────────────────────────────────────── */
    accum += sample;
    if (++summed < summands) continue;
    summed = 0;
    sample = accum / summands;
    accum = 0.0f;

    /* ── Master volume e gain ────────────────────────────────────────── */
    sample = sample / 8.0f;   /* ÷ OVERSAMPLING */
    sample *= gain;           /* × (exp(sound_vol) - 1) */

    /* ── Saída 16-bit PCM ────────────────────────────────────────────── */
    int32_t s = (int32_t)floorf(sample * 32768.0f);
    if (s >= 32768)        s = 32767;
    else if (s < -32768)   s = -32768;
    samples[out++] = (int16_t)(s & 0xFFFF);
  }

  return out;
}

/* ── Playback ────────────────────────────────────────────────────────────── */

static void (*_sfxr_cb)(bool) = NULL;

void gfxa_sfxr_play(const float params[GFXA_SFXR_PARAM_COUNT]) {
  int max_samp = 16000 * 2; /* ~2 s at 16 kHz */
  int16_t *samples = (int16_t *)malloc((size_t)max_samp * sizeof(int16_t));
  if (!samples) return;

  int total = gfxa_sfxr_generate(params, samples, max_samp, 16000);
  if (total <= 0) { free(samples); return; }

  if (_sfxr_cb) _sfxr_cb(true);
  gfx_audio_play(samples, total, 16000);
  if (_sfxr_cb) _sfxr_cb(false);

  free(samples);
}

void gfxa_sfxr_play_str(const char *param_str) {
  float params[GFXA_SFXR_PARAM_COUNT];
  if (gfxa_sfxr_parse(param_str, params) != 0) return;
  gfxa_sfxr_play(params);
}

void gfxa_sfxr_play_json(const char *json_str) {
  float params[GFXA_SFXR_PARAM_COUNT];
  if (gfxa_sfxr_parse_json(json_str, params) != 0) return;
  gfxa_sfxr_play(params);
}

void gfxa_sfxr_set_callback(void (*cb)(bool)) {
  _sfxr_cb = cb;
}
