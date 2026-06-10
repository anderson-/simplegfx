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

void gfxa_sfxr_random(float params[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_defaults(params);
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i++) {
    if (i == GFXA_SFXR_WAVE_TYPE) {
      params[i] = (float)(gfx_fast_rand() % 4);
    } else if (i == GFXA_SFXR_DUTY) {
      params[i] = 0.0f;
    } else if (i == GFXA_SFXR_LPF_FREQ || i == GFXA_SFXR_HPF_FREQ) {
      /* defaults */
    } else if (i == GFXA_SFXR_FREQ_RAMP || i == GFXA_SFXR_FREQ_DRAMP ||
               i == GFXA_SFXR_ARP_MOD || i == GFXA_SFXR_DUTY_RAMP ||
               i == GFXA_SFXR_PHA_OFFSET || i == GFXA_SFXR_PHA_RAMP ||
               i == GFXA_SFXR_LPF_RAMP || i == GFXA_SFXR_HPF_RAMP) {
      params[i] = 0.5f;
    } else {
      params[i] = (float)(gfx_fast_rand() % 1000) / 1000.0f;
    }
  }
}

void gfxa_sfxr_mutate(float params[GFXA_SFXR_PARAM_COUNT]) {
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i++) {
    if (i == GFXA_SFXR_WAVE_TYPE) continue;
    if (gfx_fast_rand() % 100 < 50) {
      float delta = ((float)(gfx_fast_rand() % 1001) / 10000.0f) - 0.05f;
      float v = params[i] + delta;
      if (v < 0.0f) v = 0.0f;
      if (v > 1.0f) v = 1.0f;
      params[i] = v;
    }
  }
}
