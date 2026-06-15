#include "presets.h"
#include "simplegfx.h"
#include "ext/term/b64.h"
#include <string.h>

static uint8_t rnd8(void) {
  return (uint8_t)gfx_fast_rand();
}

static uint8_t rng(uint8_t lo, uint8_t hi) {
  return lo + (uint8_t)(gfx_fast_rand() % (hi - lo + 1));
}

static float frnd(float range) {
  return ((float)(gfx_fast_rand() & 0x7FFF) / 32768.0f) * range;
}

static float sqr(float x) { return x * x; }
static float cube(float x) { return x * x * x; }
static float pow5(float x) { float s = sqr(x); return s * s * x; }

static void gen_pickup_coin(afx_t *p) {
  p->wave_type     = 1;
  p->base_freq     = rng(102, 230);
  p->attack        = 0;
  p->sustain_time  = rng(0, 26);
  p->decay         = rng(26, 128);
  p->sustain_punch = rng(76, 153);
  if (rnd8() & 1) {
    p->arp_speed    = rng(128, 178);
    p->arp_mod      = rng(153, 204);
  }
}

static void gen_laser_shoot(afx_t *p) {
  p->wave_type = rng(0, 2);
  if (p->wave_type == 2 && (rnd8() & 1)) p->wave_type = rng(0, 1);
  if (rng(0, 2) == 0) {
    p->base_freq  = rng(76, 230);
    p->freq_limit = rng(0, 26);
    p->freq_ramp  = rng(38, 83);
  } else {
    p->base_freq  = rng(128, 255);
    p->freq_limit = (uint8_t)((int)p->base_freq - 51 - (rnd8() % 154));
    if ((int)p->freq_limit < 51) p->freq_limit = 51;
    p->freq_ramp  = rng(83, 108);
  }
  if (p->wave_type == 1) p->duty = 255;
  if (rnd8() & 1) {
    p->duty       = rng(0, 128);
    p->duty_ramp  = rng(128, 153);
  } else {
    p->duty       = rng(102, 230);
    p->duty_ramp  = rng(38, 128);
  }
  p->attack       = 0;
  p->sustain_time = rng(26, 76);
  p->decay        = rng(0, 102);
  if (rnd8() & 1) p->sustain_punch = rng(0, 76);
  if (rng(0, 2) == 0) {
    p->pha_offset = rng(128, 153);
    p->pha_ramp   = rng(102, 128);
  }
  p->hpf_freq     = rng(0, 76);
}

static void gen_explosion(afx_t *p) {
  p->wave_type = 3;
  if (rnd8() & 1) {
    p->base_freq  = rng(3, 64);
    p->freq_ramp  = rng(115, 166);
  } else {
    p->base_freq  = rng(10, 207);
    p->freq_ramp  = rng(76, 102);
  }
  if (rng(0, 4) == 0) p->freq_ramp = 128;
  if (rng(0, 2) == 0) p->repeat_speed = rng(76, 204);
  p->attack       = 0;
  p->sustain_time = rng(26, 102);
  p->decay        = rng(0, 128);
  if (rnd8() & 1) {
    p->pha_offset = rng(89, 204);
    p->pha_ramp   = rng(89, 128);
  }
  p->sustain_punch = rng(51, 204);
  if (rnd8() & 1) {
    p->vib_strength = rng(0, 178);
    p->vib_speed    = rng(0, 153);
  }
  if (rng(0, 2) == 0) {
    p->arp_speed    = rng(153, 230);
    p->arp_mod      = rng(25, 230);
  }
}

static void gen_power_up(afx_t *p) {
  if (rnd8() & 1) {
    p->wave_type = 1;
    p->duty      = 255;
  } else {
    p->duty      = rng(0, 153);
  }
  p->base_freq    = rng(51, 128);
  if (rnd8() & 1) {
    p->freq_ramp    = rng(140, 191);
    p->repeat_speed = rng(102, 204);
  } else {
    p->freq_ramp    = rng(134, 159);
    if (rnd8() & 1) {
      p->vib_strength = rng(0, 178);
      p->vib_speed    = rng(0, 153);
    }
  }
  p->attack       = 0;
  p->sustain_time = rng(0, 102);
  p->decay        = rng(26, 128);
}

static void gen_hit_hurt(afx_t *p) {
  p->wave_type = rng(0, 2);
  if (p->wave_type == 2) p->wave_type = 3;
  if (p->wave_type == 0) p->duty = rng(0, 153);
  if (p->wave_type == 1) p->duty = 255;
  p->base_freq    = rng(51, 204);
  p->freq_ramp    = rng(38, 89);
  p->attack       = 0;
  p->sustain_time = rng(0, 26);
  p->decay        = rng(26, 76);
  if (rnd8() & 1) p->hpf_freq = rng(0, 76);
}

static void gen_jump(afx_t *p) {
  p->wave_type    = 0;
  p->duty         = rng(0, 153);
  p->base_freq    = rng(76, 153);
  p->freq_ramp    = rng(140, 166);
  p->attack       = 0;
  p->sustain_time = rng(26, 102);
  p->decay        = rng(26, 76);
  if (rnd8() & 1) p->hpf_freq = rng(0, 76);
  if (rnd8() & 1) p->lpf_freq = rng(102, 255);
}

static void gen_blip_select(afx_t *p) {
  p->wave_type = rng(0, 1);
  if (p->wave_type == 0) p->duty = rng(0, 153);
  else                    p->duty = 255;
  p->base_freq    = rng(51, 153);
  p->attack       = 0;
  p->sustain_time = rng(26, 51);
  p->decay        = rng(0, 51);
  p->hpf_freq     = 25;
}

static void gen_coin(afx_t *p) {
  base64_decode("AQAOgkJ3AIKCAAC7jgCCAIKC/4IAAIKC", (uint8_t *)p, sizeof(afx_t));
}

static void gen_piano(afx_t *p) {
  base64_decode("AQBnPsIQAIKCAoj/oa6CAIKhoS9dAJqC", (uint8_t *)p, sizeof(afx_t));
}

static void (*const generators[])(afx_t *) = {
  gen_pickup_coin, gen_laser_shoot, gen_explosion,
  gen_power_up, gen_hit_hurt, gen_jump, gen_blip_select,
  gen_coin, gen_piano
};

void gfxa_sfxr_preset(int idx, afx_t *p) {
  gfxa_sfxr_defaults(p);
  if ((unsigned)idx < GFXA_SFXR_PRESET_COUNT)
    generators[idx](p);
}

void gfxa_sfxr_random(afx_t *p) {
  gfxa_sfxr_defaults(p);

  p->wave_type = rng(0, 3);

  float freq, ramp, dramp, duty, duty_ramp;
  float vib_str, vib_spd;
  float env_attack, env_sustain, env_decay, env_punch;
  float lpf_res, lpf_freq, lpf_ramp;
  float hpf_freq, hpf_ramp;
  float pha_off, pha_ramp;
  float repeat, arp_spd, arp_mod;

  /* freq */
  if (rnd8() & 1)
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
  vib_spd = frnd(2.0f) - 1.0f;

  /* envelope */
  env_attack = cube(frnd(2.0f) - 1.0f);
  env_sustain = sqr(frnd(2.0f) - 1.0f);
  env_decay = frnd(2.0f) - 1.0f;
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
  lpf_res = frnd(2.0f) - 1.0f;
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

  p->base_freq      = (uint8_t)(freq * 255.0f + 0.5f);
  p->freq_limit     = 0;
  p->freq_ramp      = (uint8_t)((ramp + 1.0f) * 127.5f + 0.5f);
  p->freq_dramp     = (uint8_t)((dramp + 1.0f) * 127.5f + 0.5f);
  p->duty           = (uint8_t)((duty + 1.0f) * 127.5f + 0.5f);
  p->duty_ramp      = (uint8_t)((duty_ramp + 1.0f) * 127.5f + 0.5f);
  p->vib_strength   = (uint8_t)((vib_str < 0.0f ? -vib_str : vib_str) * 255.0f + 0.5f);
  p->vib_speed      = (uint8_t)((vib_spd < 0.0f ? -vib_spd : vib_spd) * 255.0f + 0.5f);
  p->attack         = (uint8_t)(env_attack * 255.0f + 0.5f);
  p->sustain_time   = (uint8_t)(env_sustain * 255.0f + 0.5f);
  p->decay          = (uint8_t)(env_decay * 255.0f + 0.5f);
  p->sustain_punch  = (uint8_t)(env_punch * 255.0f + 0.5f);
  p->lpf_resonance  = (uint8_t)((lpf_res + 1.0f) * 127.5f + 0.5f);
  p->lpf_freq       = (uint8_t)(lpf_freq * 255.0f + 0.5f);
  p->lpf_ramp       = (uint8_t)((lpf_ramp + 1.0f) * 127.5f + 0.5f);
  p->hpf_freq       = (uint8_t)(hpf_freq * 255.0f + 0.5f);
  p->hpf_ramp       = (uint8_t)((hpf_ramp + 1.0f) * 127.5f + 0.5f);
  p->pha_offset     = (uint8_t)((pha_off + 1.0f) * 127.5f + 0.5f);
  p->pha_ramp       = (uint8_t)((pha_ramp + 1.0f) * 127.5f + 0.5f);
  p->repeat_speed   = (uint8_t)((repeat < 0.0f ? 0.0f : repeat) * 255.0f + 0.5f);
  p->arp_speed      = (uint8_t)((arp_spd < 0.0f ? 0.0f : arp_spd) * 255.0f + 0.5f);
  p->arp_mod        = (uint8_t)((arp_mod + 1.0f) * 127.5f + 0.5f);
}

void gfxa_sfxr_mutate(afx_t *p) {
#define MUT(field) do { \
    if (rnd8() & 1) { \
      int v = (int)p->field + (int)(rnd8() % 51) - 25; \
      if (v < 0) v = 0; \
      if (v > 255) v = 255; \
      p->field = (uint8_t)v; \
    } \
  } while (0)

  MUT(base_freq);
  MUT(freq_ramp);
  MUT(freq_dramp);
  MUT(duty);
  MUT(duty_ramp);
  MUT(vib_strength);
  MUT(vib_speed);
  MUT(attack);
  MUT(sustain_time);
  MUT(decay);
  MUT(sustain_punch);
  MUT(lpf_resonance);
  MUT(lpf_freq);
  MUT(lpf_ramp);
  MUT(hpf_freq);
  MUT(hpf_ramp);
  MUT(pha_offset);
  MUT(pha_ramp);
  MUT(repeat_speed);
  MUT(arp_speed);
  MUT(arp_mod);

#undef MUT
}
