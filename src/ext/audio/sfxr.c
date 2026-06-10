#include "sfxr.h"
#include "simplegfx.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

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

static bool str_ieq(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    ++a; ++b;
  }
  return (*a == '\0' && *b == '\0');
}

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
  if (*p) ++p;
  return p;
}

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
      while (*p && *p != ',' && *p != '}' && *p != ']' && (unsigned char)*p > ' ') ++p;
      break;
  }
  return p;
}

static bool json_read_float(const char *json, const char *key, float *out) {
  const char *p = json;
  char buf[64];
  while (*p) {
    p = skip_ws(p);
    if (*p == '}') break;
    if (*p == ',' || *p == '{') { ++p; continue; }
    p = skip_ws(p);
    if (!*p) break;
    const char *after_key;
    bool key_matches = false;
    if (*p == '"' || *p == '\'') {
      after_key = json_read_quoted(p, buf, sizeof(buf));
      key_matches = str_ieq(buf, key);
    } else if ((unsigned char)*p > ' ') {
      const char *kp = p, *k = key;
      while (*k && tolower((unsigned char)*kp) == tolower((unsigned char)*k)) { ++kp; ++k; }
      key_matches = (!*k && ((unsigned char)*kp <= ' ' || *kp == ':'));
      if (key_matches) after_key = kp;
      else {
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
      p = json_skip_value(p);
      continue;
    }
    if (*p == '"' || *p == '\'') ++p;
    char *end = NULL;
    *out = (float)strtod(p, &end);
    if (end != p) return true;
    return false;
  }
  return false;
}

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

static const struct { const char *name; int idx; } field_map[] = {
  {"wave_type",      GFXA_SFXR_WAVE_TYPE},
  {"waveType",       GFXA_SFXR_WAVE_TYPE},
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
  {"p_vib_strength", GFXA_SFXR_VIB_STRENGTH},
  {"vib_strength",   GFXA_SFXR_VIB_STRENGTH},
  {"vib_depth",      GFXA_SFXR_VIB_STRENGTH},
  {"vibDepth",       GFXA_SFXR_VIB_STRENGTH},
  {"p_vib_speed",    GFXA_SFXR_VIB_SPEED},
  {"vib_speed",      GFXA_SFXR_VIB_SPEED},
  {"vibSpeed",       GFXA_SFXR_VIB_SPEED},
  {"p_arp_mod",      GFXA_SFXR_ARP_MOD},
  {"arp_mod",        GFXA_SFXR_ARP_MOD},
  {"arpMod",         GFXA_SFXR_ARP_MOD},
  {"p_arp_speed",    GFXA_SFXR_ARP_SPEED},
  {"arp_speed",      GFXA_SFXR_ARP_SPEED},
  {"arpSpeed",       GFXA_SFXR_ARP_SPEED},
  {"p_duty",         GFXA_SFXR_DUTY},
  {"duty",           GFXA_SFXR_DUTY},
  {"square_duty",    GFXA_SFXR_DUTY},
  {"squareDuty",     GFXA_SFXR_DUTY},
  {"p_duty_ramp",    GFXA_SFXR_DUTY_RAMP},
  {"duty_ramp",      GFXA_SFXR_DUTY_RAMP},
  {"duty_sweep",     GFXA_SFXR_DUTY_RAMP},
  {"dutySweep",      GFXA_SFXR_DUTY_RAMP},
  {"p_repeat_speed", GFXA_SFXR_REPEAT_SPEED},
  {"repeat_speed",   GFXA_SFXR_REPEAT_SPEED},
  {"repeatSpeed",    GFXA_SFXR_REPEAT_SPEED},
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
  {"p_hpf_freq",     GFXA_SFXR_HPF_FREQ},
  {"hpf_freq",       GFXA_SFXR_HPF_FREQ},
  {"hpfFreq",        GFXA_SFXR_HPF_FREQ},
  {"hpf_cutoff",     GFXA_SFXR_HPF_FREQ},
  {"hpfCutoff",      GFXA_SFXR_HPF_FREQ},
  {"p_hpf_ramp",     GFXA_SFXR_HPF_RAMP},
  {"hpf_ramp",       GFXA_SFXR_HPF_RAMP},
  {"hpfRamp",        GFXA_SFXR_HPF_RAMP},
  {"sound_vol",      GFXA_SFXR_SOUND_VOL},
  {"soundVol",       GFXA_SFXR_SOUND_VOL},
  {NULL, 0}
};

int gfxa_sfxr_parse_json(const char *json, float params[GFXA_SFXR_PARAM_COUNT]) {
  if (!json || !json[0]) return 1;
  gfxa_sfxr_defaults(params);
  for (int i = 0; field_map[i].name; ++i) {
    float v;
    if (json_read_float(json, field_map[i].name, &v)) {
      if (is_signed_param(field_map[i].idx))
        v = (v + 1.0f) / 2.0f;
      params[field_map[i].idx] = clampf(v, 0.0f, 1.0f);
    }
  }
  return 0;
}

void gfxa_sfxr_defaults(float params[GFXA_SFXR_PARAM_COUNT]) {
  params[GFXA_SFXR_WAVE_TYPE]      = 0.0f;
  params[GFXA_SFXR_ATTACK]         = 0.0f;
  params[GFXA_SFXR_SUSTAIN_TIME]   = 0.3f;
  params[GFXA_SFXR_SUSTAIN_PUNCH]  = 0.0f;
  params[GFXA_SFXR_DECAY]          = 0.4f;
  params[GFXA_SFXR_BASE_FREQ]      = 0.3f;
  params[GFXA_SFXR_FREQ_LIMIT]     = 0.0f;
  params[GFXA_SFXR_FREQ_RAMP]      = 0.5f;
  params[GFXA_SFXR_FREQ_DRAMP]     = 0.5f;
  params[GFXA_SFXR_VIB_STRENGTH]   = 0.0f;
  params[GFXA_SFXR_VIB_SPEED]      = 0.0f;
  params[GFXA_SFXR_ARP_MOD]        = 0.5f;
  params[GFXA_SFXR_ARP_SPEED]      = 0.0f;
  params[GFXA_SFXR_DUTY]           = 0.0f;
  params[GFXA_SFXR_DUTY_RAMP]      = 0.5f;
  params[GFXA_SFXR_REPEAT_SPEED]   = 0.0f;
  params[GFXA_SFXR_PHA_OFFSET]     = 0.5f;
  params[GFXA_SFXR_PHA_RAMP]       = 0.5f;
  params[GFXA_SFXR_LPF_FREQ]       = 1.0f;
  params[GFXA_SFXR_LPF_RAMP]       = 0.5f;
  params[GFXA_SFXR_LPF_RESONANCE]  = 0.0f;
  params[GFXA_SFXR_HPF_FREQ]       = 0.0f;
  params[GFXA_SFXR_HPF_RAMP]       = 0.5f;
  params[GFXA_SFXR_SOUND_VOL]      = 0.5f;
}

struct sfxr_state {
  float period;
  float period_max;
  float pmul;
  float pslide;
  float duty;
  float dslide;
  float arp_mult;
  int arp_time;
  int rpt_elapsed;
  int rpt_time;
  int freq_cut;
  int phase;
  int wave_shape;
  float fltw;
  float fltw_d;
  float fltdmp;
  float fltp;
  float fltdp;
  float flthp;
  float flthp_d;
  float fltphp;
  int en_lpf;
  float vib_speed;
  float vib_amp;
  float vib_phase;
  int env_len[3];
  int env_stage;
  int env_elapsed;
  float env_punch;
  float flg_buf[1024];
  float flg_off;
  float flg_sl;
  int flg_pos;
  float gain;
  float noise[32];
  int summands;
  int summed;
  float accum;
  int sample_rate;
  int t;
  int done;
};

struct sfxr_state *gfxa_sfxr_create(const float params[GFXA_SFXR_PARAM_COUNT]) {
  struct sfxr_state *s = calloc(1, sizeof(*s));
  if (!s) return NULL;

  int wt = (int)clampf(params[GFXA_SFXR_WAVE_TYPE], 0, 4);
  float pa  = clampf(params[GFXA_SFXR_ATTACK], 0, 1);
  float pst = clampf(params[GFXA_SFXR_SUSTAIN_TIME], 0, 1);
  float ppu = clampf(params[GFXA_SFXR_SUSTAIN_PUNCH], 0, 1);
  float pd  = clampf(params[GFXA_SFXR_DECAY], 0, 1);
  float pbf = clampf(params[GFXA_SFXR_BASE_FREQ], 0, 1);
  float pfl = clampf(params[GFXA_SFXR_FREQ_LIMIT], 0, 1);
  float pfr = norm_signed(clampf(params[GFXA_SFXR_FREQ_RAMP], 0, 1));
  float pfd = norm_signed(clampf(params[GFXA_SFXR_FREQ_DRAMP], 0, 1));
  float pvstrength = clampf(params[GFXA_SFXR_VIB_STRENGTH], 0, 1);
  float pvspeed    = clampf(params[GFXA_SFXR_VIB_SPEED], 0, 1);
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

  s->sample_rate = 16000;
  s->wave_shape = wt;
  s->fltw = plc * plc * plc * 0.1f;
  s->en_lpf = (plc != 1.0f);
  s->fltw_d = 1.0f + plrm * 0.0001f;
  s->fltdmp = 5.0f / (1.0f + plr * plr * 20.0f) * (0.01f + s->fltw);
  if (s->fltdmp > 0.8f) s->fltdmp = 0.8f;
  s->fltp = 0.0f;
  s->fltdp = 0.0f;
  s->flthp = phc * phc * 0.1f;
  s->flthp_d = 1.0f + phrm * 0.0003f;
  s->fltphp = 0.0f;
  s->vib_speed = pvspeed * pvspeed * 0.01f;
  s->vib_amp   = pvstrength * 0.5f;
  s->vib_phase = 0.0f;
  s->env_len[0] = (int)(pa * pa * 100000.0f);
  s->env_len[1] = (int)(pst * pst * 100000.0f);
  s->env_len[2] = (int)(pd * pd * 100000.0f);
  s->env_stage = 0;
  s->env_elapsed = 0;
  s->env_punch = ppu;
  memset(s->flg_buf, 0, sizeof(s->flg_buf));
  s->flg_off = (pho >= 0.0f ? 1.0f : -1.0f) * (pho * pho) * 1020.0f;
  s->flg_sl  = (phs >= 0.0f ? 1.0f : -1.0f) * (phs * phs);
  s->flg_pos = 0;
  s->rpt_time = (int)((1.0f - prp) * (1.0f - prp) * 20000.0f + 32.0f);
  if (prp <= 0.0f) s->rpt_time = 0;
  s->gain = expf(psndvol) - 1.0f;
  s->period = 100.0f / (pbf * pbf + 0.001f);
  s->period_max = 100.0f / (pfl * pfl + 0.001f);
  s->freq_cut = (pfl > 0.0f);
  s->pmul = 1.0f - (pfr * pfr * pfr) * 0.01f;
  s->pslide = -(pfd * pfd * pfd) * 0.000001f;
  s->duty = 0.5f - pdu * 0.5f;
  s->dslide = -pds * 0.00005f;
  if (parp_mod >= 0.0f)
    s->arp_mult = 1.0f - (parp_mod * parp_mod) * 0.9f;
  else
    s->arp_mult = 1.0f + (parp_mod * parp_mod) * 10.0f;
  s->arp_time = (int)((1.0f - parp_spd) * (1.0f - parp_spd) * 20000.0f + 32.0f);
  if (parp_spd >= 1.0f) s->arp_time = 0;
  s->rpt_elapsed = 0;
  for (int i = 0; i < 32; i++)
    s->noise[i] = (float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f;
  s->summands = 44100 / s->sample_rate;
  if (s->summands < 1) s->summands = 1;
  s->summed = 0;
  s->accum = 0.0f;
  s->phase = 0;
  s->t = 0;
  return s;
}

int gfxa_sfxr_read(struct sfxr_state *s, int16_t *buf, int n) {
  if (s->done) return 0;
  int out = 0;
  while (out < n) {
    /* Repeat */
    if (s->rpt_time != 0) {
      s->rpt_elapsed++;
      if (s->rpt_elapsed >= s->rpt_time) {
        s->rpt_elapsed = 0;
        /* re-initForRepeat would need stored params — skipped for now */
      }
    }
    /* Arpeggio */
    if (s->arp_time != 0 && s->t >= s->arp_time) {
      s->arp_time = 0;
      s->period *= s->arp_mult;
    }
    /* Frequency slide */
    s->pmul += s->pslide;
    s->period *= s->pmul;
    if (s->period > s->period_max) {
      s->period = s->period_max;
      if (s->freq_cut) { s->done = 1; break; }
    }
    /* Vibrato */
    float rfperiod = s->period;
    if (s->vib_amp > 0.0f) {
      s->vib_phase += s->vib_speed;
      rfperiod = s->period * (1.0f + sinf(s->vib_phase) * s->vib_amp);
    }
    int iperiod = (int)rfperiod;
    if (iperiod < 8) iperiod = 8;
    /* Duty */
    s->duty += s->dslide;
    if (s->duty < 0.0f) s->duty = 0.0f;
    if (s->duty > 0.5f) s->duty = 0.5f;
    /* Envelope */
    if (++s->env_elapsed > s->env_len[s->env_stage]) {
      s->env_elapsed = 0;
      if (++s->env_stage > 2) { s->done = 1; break; }
    }
    float envf = s->env_len[s->env_stage] > 0
      ? (float)s->env_elapsed / s->env_len[s->env_stage] : 0.0f;
    float ev;
    if (s->env_stage == 0) ev = envf;
    else if (s->env_stage == 1) ev = 1.0f + (1.0f - envf) * 2.0f * s->env_punch;
    else ev = 1.0f - envf;
    /* Flanger */
    s->flg_off += s->flg_sl;
    int iphase = (int)fabsf(floorf(s->flg_off));
    if (iphase > 1023) iphase = 1023;
    /* HPF slide */
    s->flthp *= s->flthp_d;
    if (s->flthp < 0.00001f) s->flthp = 0.00001f;
    if (s->flthp > 0.1f) s->flthp = 0.1f;
    /* 8x oversampling */
    float sample = 0.0f;
    for (int si = 0; si < 8; si++) {
      float ss = 0.0f;
      s->phase++;
      if (s->phase >= iperiod) {
        s->phase %= iperiod;
        if (s->wave_shape == GFXA_SFXR_WAVE_NOISE)
          for (int j = 0; j < 32; j++)
            s->noise[j] = (float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f;
      }
      float fp = (float)s->phase / iperiod;
      switch (s->wave_shape) {
        case 0: ss = (fp < s->duty) ? 0.5f : -0.5f; break;
        case 1:
          if (fp < s->duty) ss = -1.0f + 2.0f * fp / s->duty;
          else ss = 1.0f - 2.0f * (fp - s->duty) / (1.0f - s->duty);
          break;
        case 2: ss = sinf(fp * 2.0f * (float)M_PI); break;
        case 3: ss = s->noise[(s->phase * 32 / iperiod) % 32]; break;
      }
      float pp = s->fltp;
      s->fltw *= s->fltw_d;
      if (s->fltw < 0.0f) s->fltw = 0.0f;
      if (s->fltw > 0.1f) s->fltw = 0.1f;
      if (s->en_lpf) {
        s->fltdp += (ss - s->fltp) * s->fltw;
        s->fltdp -= s->fltdp * s->fltdmp;
      } else {
        s->fltp = ss;
        s->fltdp = 0.0f;
      }
      s->fltp += s->fltdp;
      s->fltphp += s->fltp - pp;
      s->fltphp -= s->fltphp * s->flthp;
      ss = s->fltphp;
      s->flg_buf[s->flg_pos & 1023] = ss;
      ss += s->flg_buf[(s->flg_pos - iphase + 1024) & 1023];
      s->flg_pos = (s->flg_pos + 1) & 1023;
      sample += ss * ev;
    }
    /* Downsample */
    s->accum += sample;
    if (++s->summed < s->summands) { s->t++; continue; }
    s->summed = 0;
    sample = s->accum / s->summands;
    s->accum = 0.0f;
    /* Gain and output */
    sample = sample / 8.0f * s->gain;
    int32_t sample_out = (int32_t)floorf(sample * 32768.0f);
    if (sample_out >= 32768) sample_out = 32767;
    else if (sample_out < -32768) sample_out = -32768;
    buf[out++] = (int16_t)(sample_out & 0xFFFF);
    s->t++;
  }
  return out;
}

static void (*_sfxr_cb)(bool) = NULL;

static int sfxr_fill_wrapper(int16_t *buf, int n, void *user) {
  return gfxa_sfxr_read((struct sfxr_state *)user, buf, n);
}

void gfxa_sfxr_destroy(struct sfxr_state *s) {
  free(s);
}

void gfxa_sfxr_play(const float params[GFXA_SFXR_PARAM_COUNT]) {
  struct sfxr_state *s = gfxa_sfxr_create(params);
  if (!s) return;
  if (_sfxr_cb) _sfxr_cb(true);
  gfx_audio_play_stream(sfxr_fill_wrapper, s, s->sample_rate);
  if (_sfxr_cb) _sfxr_cb(false);
  gfxa_sfxr_destroy(s);
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
