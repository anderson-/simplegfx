#include "sfxr_presets.h"
#include "simplegfx.h"
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static float frnd(float range) {
  return ((float)(gfx_fast_rand() & 0x7FFF) / 32768.0f) * range;
}

static int rnd(int max) {
  return (int)(gfx_fast_rand() % (max + 1));
}

static float sqr(float x) { return x * x; }
static float cube(float x) { return x * x * x; }
static float pow5(float x) { float s = sqr(x); return s * s * x; }

static float rndr(float a, float b) { return a + frnd(b - a); }

static float s2c(float js_signed) {
  return (js_signed + 1.0f) / 2.0f;
}

/* ── Geradores ───────────────────────────────────────────────────────────── */

static void gen_pickup_coin(float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(p);
  p[GFXA_SFXR_WAVE_TYPE]    = 1; /* SAWTOOTH */
  p[GFXA_SFXR_BASE_FREQ]    = 0.4f + frnd(0.5f);
  p[GFXA_SFXR_ATTACK]       = 0.0f;
  p[GFXA_SFXR_SUSTAIN_TIME] = frnd(0.1f);
  p[GFXA_SFXR_DECAY]        = 0.1f + frnd(0.4f);
  p[GFXA_SFXR_SUSTAIN_PUNCH]= 0.3f + frnd(0.3f);
  if (rnd(1)) {
    float arp_speed = 0.5f + frnd(0.2f);
    float arp_mod   = 0.2f + frnd(0.4f);
    p[GFXA_SFXR_ARP_SPEED]   = arp_speed;
    p[GFXA_SFXR_ARP_MOD]     = s2c(arp_mod);
  }
}

static void gen_laser_shoot(float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(p);
  p[GFXA_SFXR_WAVE_TYPE] = (float)rnd(2);
  if (p[GFXA_SFXR_WAVE_TYPE] == 2 && rnd(1))
    p[GFXA_SFXR_WAVE_TYPE] = (float)rnd(1);
  if (rnd(2) == 0) {
    p[GFXA_SFXR_BASE_FREQ]  = 0.3f + frnd(0.6f);
    p[GFXA_SFXR_FREQ_LIMIT] = frnd(0.1f);
    float ramp = -0.35f - frnd(0.3f);
    p[GFXA_SFXR_FREQ_RAMP]  = s2c(ramp);
  } else {
    p[GFXA_SFXR_BASE_FREQ]  = 0.5f + frnd(0.5f);
    p[GFXA_SFXR_FREQ_LIMIT] = p[GFXA_SFXR_BASE_FREQ] - 0.2f - frnd(0.6f);
    if (p[GFXA_SFXR_FREQ_LIMIT] < 0.2f) p[GFXA_SFXR_FREQ_LIMIT] = 0.2f;
    float ramp = -0.15f - frnd(0.2f);
    p[GFXA_SFXR_FREQ_RAMP]  = s2c(ramp);
  }
  if (p[GFXA_SFXR_WAVE_TYPE] == 1) p[GFXA_SFXR_DUTY] = 1.0f;
  if (rnd(1)) {
    p[GFXA_SFXR_DUTY]     = frnd(0.5f);
    p[GFXA_SFXR_DUTY_RAMP]= s2c(frnd(0.2f));
  } else {
    p[GFXA_SFXR_DUTY]     = 0.4f + frnd(0.5f);
    p[GFXA_SFXR_DUTY_RAMP]= s2c(-frnd(0.7f));
  }
  p[GFXA_SFXR_ATTACK]       = 0.0f;
  p[GFXA_SFXR_SUSTAIN_TIME] = 0.1f + frnd(0.2f);
  p[GFXA_SFXR_DECAY]        = frnd(0.4f);
  if (rnd(1)) p[GFXA_SFXR_SUSTAIN_PUNCH] = frnd(0.3f);
  if (rnd(2) == 0) {
    p[GFXA_SFXR_PHA_OFFSET] = s2c(frnd(0.2f));
    p[GFXA_SFXR_PHA_RAMP]   = s2c(-frnd(0.2f));
  }
  p[GFXA_SFXR_HPF_FREQ] = frnd(0.3f);
}

static void gen_explosion(float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(p);
  p[GFXA_SFXR_WAVE_TYPE] = 3; /* NOISE */
  if (rnd(1)) {
    p[GFXA_SFXR_BASE_FREQ] = sqr(0.1f + frnd(0.4f));
    p[GFXA_SFXR_FREQ_RAMP] = s2c(-0.1f + frnd(0.4f));
  } else {
    p[GFXA_SFXR_BASE_FREQ] = sqr(0.2f + frnd(0.7f));
    p[GFXA_SFXR_FREQ_RAMP] = s2c(-0.2f - frnd(0.2f));
  }
  if (rnd(4) == 0) p[GFXA_SFXR_FREQ_RAMP] = 0.5f;
  if (rnd(2) == 0) p[GFXA_SFXR_REPEAT_SPEED] = 0.3f + frnd(0.5f);
  p[GFXA_SFXR_ATTACK]       = 0.0f;
  p[GFXA_SFXR_SUSTAIN_TIME] = 0.1f + frnd(0.3f);
  p[GFXA_SFXR_DECAY]        = frnd(0.5f);
  if (rnd(1)) {
    p[GFXA_SFXR_PHA_OFFSET] = s2c(-0.3f + frnd(0.9f));
    p[GFXA_SFXR_PHA_RAMP]   = s2c(-frnd(0.3f));
  }
  p[GFXA_SFXR_SUSTAIN_PUNCH] = 0.2f + frnd(0.6f);
  if (rnd(1)) {
    p[GFXA_SFXR_VIB_STRENGTH] = frnd(0.7f);
    p[GFXA_SFXR_VIB_SPEED]    = frnd(0.6f);
  }
  if (rnd(2) == 0) {
    float arp_speed = 0.6f + frnd(0.3f);
    float arp_mod   = 0.8f - frnd(1.6f);
    p[GFXA_SFXR_ARP_SPEED]   = arp_speed;
    p[GFXA_SFXR_ARP_MOD]     = s2c(arp_mod);
  }
}

static void gen_power_up(float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(p);
  if (rnd(1)) {
    p[GFXA_SFXR_WAVE_TYPE] = 1;
    p[GFXA_SFXR_DUTY]      = 1.0f;
  } else {
    p[GFXA_SFXR_DUTY] = frnd(0.6f);
  }
  p[GFXA_SFXR_BASE_FREQ] = 0.2f + frnd(0.3f);
  if (rnd(1)) {
    p[GFXA_SFXR_FREQ_RAMP]    = s2c(0.1f + frnd(0.4f));
    p[GFXA_SFXR_REPEAT_SPEED] = 0.4f + frnd(0.4f);
  } else {
    p[GFXA_SFXR_FREQ_RAMP] = s2c(0.05f + frnd(0.2f));
    if (rnd(1)) {
      p[GFXA_SFXR_VIB_STRENGTH] = frnd(0.7f);
      p[GFXA_SFXR_VIB_SPEED]    = frnd(0.6f);
    }
  }
  p[GFXA_SFXR_ATTACK]       = 0.0f;
  p[GFXA_SFXR_SUSTAIN_TIME] = frnd(0.4f);
  p[GFXA_SFXR_DECAY]        = 0.1f + frnd(0.4f);
}

static void gen_hit_hurt(float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(p);
  p[GFXA_SFXR_WAVE_TYPE] = (float)rnd(2);
  if (p[GFXA_SFXR_WAVE_TYPE] == 2) p[GFXA_SFXR_WAVE_TYPE] = 3;
  if (p[GFXA_SFXR_WAVE_TYPE] == 0) p[GFXA_SFXR_DUTY] = frnd(0.6f);
  if (p[GFXA_SFXR_WAVE_TYPE] == 1) p[GFXA_SFXR_DUTY] = 1.0f;
  p[GFXA_SFXR_BASE_FREQ]  = 0.2f + frnd(0.6f);
  p[GFXA_SFXR_FREQ_RAMP]  = s2c(-0.3f - frnd(0.4f));
  p[GFXA_SFXR_ATTACK]     = 0.0f;
  p[GFXA_SFXR_SUSTAIN_TIME] = frnd(0.1f);
  p[GFXA_SFXR_DECAY]      = 0.1f + frnd(0.2f);
  if (rnd(1)) p[GFXA_SFXR_HPF_FREQ] = frnd(0.3f);
}

static void gen_jump(float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(p);
  p[GFXA_SFXR_WAVE_TYPE] = 0;
  p[GFXA_SFXR_DUTY]      = frnd(0.6f);
  p[GFXA_SFXR_BASE_FREQ] = 0.3f + frnd(0.3f);
  p[GFXA_SFXR_FREQ_RAMP] = s2c(0.1f + frnd(0.2f));
  p[GFXA_SFXR_ATTACK]    = 0.0f;
  p[GFXA_SFXR_SUSTAIN_TIME] = 0.1f + frnd(0.3f);
  p[GFXA_SFXR_DECAY]     = 0.1f + frnd(0.2f);
  if (rnd(1)) p[GFXA_SFXR_HPF_FREQ] = frnd(0.3f);
  if (rnd(1)) p[GFXA_SFXR_LPF_FREQ] = 1.0f - frnd(0.6f);
}

static void gen_blip_select(float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(p);
  p[GFXA_SFXR_WAVE_TYPE] = (float)rnd(1);
  if (p[GFXA_SFXR_WAVE_TYPE] == 0)
    p[GFXA_SFXR_DUTY] = frnd(0.6f);
  else
    p[GFXA_SFXR_DUTY] = 1.0f;
  p[GFXA_SFXR_BASE_FREQ]    = 0.2f + frnd(0.4f);
  p[GFXA_SFXR_ATTACK]       = 0.0f;
  p[GFXA_SFXR_SUSTAIN_TIME] = 0.1f + frnd(0.1f);
  p[GFXA_SFXR_DECAY]        = frnd(0.2f);
  p[GFXA_SFXR_HPF_FREQ]     = 0.1f;
}

static void gen_coin(float p[GFXA_SFXR_PARAM_COUNT]) {
  if (gfxa_sfxr_from_base64("VAPtgrAttAA2vAtAtt/tAAtt", p) != 0)
    gfxa_sfxr_defaults(p);
}

/* Tabela de geradores */
static void (*const generators[])(float *) = {
  gen_pickup_coin, gen_laser_shoot, gen_explosion,
  gen_power_up, gen_hit_hurt, gen_jump, gen_blip_select,
  gen_coin
};

static const char *preset_names[] = {
  "Pickup/Coin", "Laser/Shoot", "Explosion", "Power-up",
  "Hit/Hurt", "Jump", "Blip/Select", "Coin"
};

/* ── API pública ─────────────────────────────────────────────────────────── */

void gfxa_sfxr_preset(int idx, float params[GFXA_SFXR_PARAM_COUNT]) {
  if ((unsigned)idx < GFXA_SFXR_PRESET_COUNT)
    generators[idx](params);
  else
    gfxa_sfxr_defaults(params);
}

const char *gfxa_sfxr_preset_name(int idx) {
  if ((unsigned)idx < GFXA_SFXR_PRESET_COUNT)
    return preset_names[idx];
  return NULL;
}

/* ── Random: tradução literal do Params.prototype.random do jsfxr ──────── */

void gfxa_sfxr_random(float params[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(params);

  params[GFXA_SFXR_WAVE_TYPE] = (float)rnd(3);

  float freq, ramp, dramp, duty, duty_ramp;
  float vib_str, vib_spd;
  float env_attack, env_sustain, env_decay, env_punch;
  float lpf_res, lpf_freq, lpf_ramp;
  float hpf_freq, hpf_ramp;
  float pha_off, pha_ramp;
  float repeat, arp_spd, arp_mod;

  /* freq */
  if (rnd(1))
    freq = cube(frnd(2.0f) - 1.0f) + 0.5f;
  else
    freq = sqr(frnd(1.0f));
  if (freq < 0.0f) freq = 0.0f;
  if (freq > 1.0f) freq = 1.0f;

  /* freq_ramp */
  ramp = pow5(frnd(2.0f) - 1.0f);
  if (freq > 0.7f && ramp > 0.2f) ramp = -ramp;
  if (freq < 0.2f && ramp < -0.05f) ramp = -ramp;

  /* freq_dramp */
  dramp = cube(frnd(2.0f) - 1.0f);

  /* duty */
  duty = frnd(2.0f) - 1.0f;
  duty_ramp = cube(frnd(2.0f) - 1.0f);

  /* vibrato */
  vib_str = cube(frnd(2.0f) - 1.0f);
  vib_spd = rndr(-1.0f, 1.0f);

  /* envelope */
  env_attack = cube(rndr(-1.0f, 1.0f));
  env_sustain = sqr(rndr(-1.0f, 1.0f));
  env_decay = rndr(-1.0f, 1.0f);
  env_punch = sqr(frnd(0.8f));

  if (env_attack + env_sustain + env_decay < 0.2f) {
    env_sustain += 0.2f + frnd(0.3f);
    env_decay   += 0.2f + frnd(0.3f);
  }
  /* clamp envelope to [0,1] */
  if (env_attack  < 0.0f) env_attack  = 0.0f;
  if (env_sustain < 0.0f) env_sustain = 0.0f;
  if (env_decay   < 0.0f) env_decay   = 0.0f;
  if (env_attack  > 1.0f) env_attack  = 1.0f;
  if (env_sustain > 1.0f) env_sustain = 1.0f;
  if (env_decay   > 1.0f) env_decay   = 1.0f;

  /* filter */
  lpf_res = rndr(-1.0f, 1.0f);
  lpf_freq = 1.0f - cube(frnd(1.0f));
  lpf_ramp = cube(frnd(2.0f) - 1.0f);
  if (lpf_freq < 0.1f && lpf_ramp < -0.05f) lpf_ramp = -lpf_ramp;

  hpf_freq = pow5(frnd(1.0f));
  hpf_ramp = pow5(frnd(2.0f) - 1.0f);

  /* phaser */
  pha_off = cube(frnd(2.0f) - 1.0f);
  pha_ramp = cube(frnd(2.0f) - 1.0f);

  /* repeat / arp */
  repeat  = frnd(2.0f) - 1.0f;
  arp_spd = frnd(2.0f) - 1.0f;
  arp_mod = frnd(2.0f) - 1.0f;

  /* ── Grava no array C ──────────────────────────────────────────────── */
  params[GFXA_SFXR_BASE_FREQ]     = freq;
  params[GFXA_SFXR_FREQ_LIMIT]    = 0.0f;
  params[GFXA_SFXR_FREQ_RAMP]     = s2c(ramp);
  params[GFXA_SFXR_FREQ_DRAMP]    = s2c(dramp);
  params[GFXA_SFXR_DUTY]          = s2c(duty);
  params[GFXA_SFXR_DUTY_RAMP]     = s2c(duty_ramp);
  params[GFXA_SFXR_VIB_STRENGTH]  = vib_str < 0.0f ? -vib_str : vib_str;
  params[GFXA_SFXR_VIB_SPEED]     = vib_spd < 0.0f ? -vib_spd : vib_spd;
  params[GFXA_SFXR_ATTACK]        = env_attack;
  params[GFXA_SFXR_SUSTAIN_TIME]  = env_sustain;
  params[GFXA_SFXR_DECAY]         = env_decay;
  params[GFXA_SFXR_SUSTAIN_PUNCH] = env_punch;
  params[GFXA_SFXR_LPF_RESONANCE] = s2c(lpf_res);
  params[GFXA_SFXR_LPF_FREQ]      = lpf_freq;
  params[GFXA_SFXR_LPF_RAMP]      = s2c(lpf_ramp);
  params[GFXA_SFXR_HPF_FREQ]      = hpf_freq;
  params[GFXA_SFXR_HPF_RAMP]      = s2c(hpf_ramp);
  params[GFXA_SFXR_PHA_OFFSET]    = s2c(pha_off);
  params[GFXA_SFXR_PHA_RAMP]      = s2c(pha_ramp);
  params[GFXA_SFXR_REPEAT_SPEED]  = repeat < 0.0f ? 0.0f : repeat;
  params[GFXA_SFXR_ARP_SPEED]     = arp_spd < 0.0f ? 0.0f : arp_spd;
  params[GFXA_SFXR_ARP_MOD]       = s2c(arp_mod);
}

/* ── Mutate: tradução do Params.prototype.mutate do jsfxr ─────────────── */

void gfxa_sfxr_mutate(float params[GFXA_SFXR_PARAM_COUNT]) {
  /* Helper: com 50% de chance aplica delta em v, clamp [0,1] */
#define MUT(p) do { \
    if (rnd(1)) { \
      float v = params[p] + frnd(0.1f) - 0.05f; \
      if (v < 0.0f) v = 0.0f; \
      if (v > 1.0f) v = 1.0f; \
      params[p] = v; \
    } \
  } while (0)

  MUT(GFXA_SFXR_BASE_FREQ);
  MUT(GFXA_SFXR_FREQ_RAMP);
  MUT(GFXA_SFXR_FREQ_DRAMP);
  MUT(GFXA_SFXR_DUTY);
  MUT(GFXA_SFXR_DUTY_RAMP);
  MUT(GFXA_SFXR_VIB_STRENGTH);
  MUT(GFXA_SFXR_VIB_SPEED);
  MUT(GFXA_SFXR_ATTACK);
  MUT(GFXA_SFXR_SUSTAIN_TIME);
  MUT(GFXA_SFXR_DECAY);
  MUT(GFXA_SFXR_SUSTAIN_PUNCH);
  MUT(GFXA_SFXR_LPF_RESONANCE);
  MUT(GFXA_SFXR_LPF_FREQ);
  MUT(GFXA_SFXR_LPF_RAMP);
  MUT(GFXA_SFXR_HPF_FREQ);
  MUT(GFXA_SFXR_HPF_RAMP);
  MUT(GFXA_SFXR_PHA_OFFSET);
  MUT(GFXA_SFXR_PHA_RAMP);
  MUT(GFXA_SFXR_REPEAT_SPEED);
  MUT(GFXA_SFXR_ARP_SPEED);
  MUT(GFXA_SFXR_ARP_MOD);

#undef MUT
}
