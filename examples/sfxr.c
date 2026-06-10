#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simplegfx.h"
#include "sfxr.h"

/* ── Helpers de随机 ─────────────────────────────────────────────────────── */

/* frnd(range)  = Math.random() * range  → [0, range) */
static float frnd(float range) {
  return ((float)(gfx_fast_rand() & 0x7FFF) / 32768.0f) * range;
}

/* rnd(max) = Math.floor(Math.random() * (max + 1)) → [0, max] */
static int rnd(int max) {
  return (int)(gfx_fast_rand() % (max + 1));
}

/* sqr(x) = x*x */
static float sqr(float x) { return x * x; }

/* Converte valor signed JS [-1,1] para armazenamento C [0,1] (0.5=centro) */
static float s2c(float js_signed) {
  return (js_signed + 1.0f) / 2.0f;
}

/* ── Geradores de presets (implementam os Params.prototype do jsfxr) ──── */

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
  if (rnd(4) == 0) p[GFXA_SFXR_FREQ_RAMP] = 0.5f; /* centro */
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
    p[GFXA_SFXR_WAVE_TYPE] = 1; /* SAWTOOTH */
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
  if (p[GFXA_SFXR_WAVE_TYPE] == 2) p[GFXA_SFXR_WAVE_TYPE] = 3; /* SINE→NOISE */
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
  p[GFXA_SFXR_WAVE_TYPE] = 0; /* SQUARE */
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
  p[GFXA_SFXR_WAVE_TYPE] = (float)rnd(1); /* 0=SQUARE ou 1=SAWTOOTH */
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

/* Tabela de geradores — mesma ordem dos nomes abaixo */
static void (*const generators[])(float *) = {
  gen_pickup_coin, gen_laser_shoot, gen_explosion,
  gen_power_up, gen_hit_hurt, gen_jump, gen_blip_select
};

/* ── JSON da moeda (formato jsfxr/bfxr) ──────────────────────────────────── */
static const char *coin_json =
  "{"
  "\"oldParams\":true,"
  "\"wave_type\":1,"
  "\"p_env_attack\":0,"
  "\"p_env_sustain\":0.05843462841353824,"
  "\"p_env_punch\":0.5158931942184259,"
  "\"p_env_decay\":0.2636346084350746,"
  "\"p_base_freq\":0.4756746513221243,"
  "\"p_freq_limit\":0,"
  "\"p_freq_ramp\":0,"
  "\"p_freq_dramp\":0,"
  "\"p_vib_strength\":0,"
  "\"p_vib_speed\":0,"
  "\"p_arp_mod\":0.44704649632387,"
  "\"p_arp_speed\":0.5638528206466059,"
  "\"p_duty\":0,"
  "\"p_duty_ramp\":0,"
  "\"p_repeat_speed\":0,"
  "\"p_pha_offset\":0,"
  "\"p_pha_ramp\":0,"
  "\"p_lpf_freq\":1,"
  "\"p_lpf_ramp\":0,"
  "\"p_lpf_resonance\":0,"
  "\"p_hpf_freq\":0,"
  "\"p_hpf_ramp\":0"
  "}";

static const char *names[] = {
  "Pickup/Coin", "Laser/Shoot", "Explosion", "Power-up",
  "Hit/Hurt", "Jump", "Blip/Select",
  "Coin (JSON)", "Random", "Mutate"
};

#define N_GEN (sizeof(generators)/sizeof(generators[0]))
#define N     (sizeof(names)/sizeof(names[0]))
#define IDX_COIN    (N_GEN)       /* 7 */
#define IDX_RANDOM  8
#define IDX_MUTATE  9

/* Buffer para parâmetros aleatórios (persiste entre mutações) */
static float rand_params[GFXA_SFXR_PARAM_COUNT];
static int sel = 0;

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void gen_random_params(void) {
  gfxa_sfxr_defaults(rand_params);
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i++) {
    if (i == GFXA_SFXR_WAVE_TYPE) {
      rand_params[i] = (float)(gfx_fast_rand() % 4);
    } else if (i == GFXA_SFXR_DUTY) {
      rand_params[i] = 0.0f;
    } else if (i == GFXA_SFXR_LPF_FREQ ||
               i == GFXA_SFXR_HPF_FREQ) {
      /* defaults */
    } else if (i == GFXA_SFXR_FREQ_RAMP ||
               i == GFXA_SFXR_FREQ_DRAMP ||
               i == GFXA_SFXR_ARP_MOD ||
               i == GFXA_SFXR_DUTY_RAMP ||
               i == GFXA_SFXR_PHA_OFFSET ||
               i == GFXA_SFXR_PHA_RAMP ||
               i == GFXA_SFXR_LPF_RAMP ||
               i == GFXA_SFXR_HPF_RAMP) {
      rand_params[i] = 0.5f;
    } else {
      rand_params[i] = (float)(gfx_fast_rand() % 1000) / 1000.0f;
    }
  }
}

static void mutate_params(void) {
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i++) {
    if (i == GFXA_SFXR_WAVE_TYPE) continue;
    if (gfx_fast_rand() % 100 < 50) {
      float delta = ((float)(gfx_fast_rand() % 1001) / 10000.0f) - 0.05f;
      float v = rand_params[i] + delta;
      if (v < 0.0f) v = 0.0f;
      if (v > 1.0f) v = 1.0f;
      rand_params[i] = v;
    }
  }
}

/* ── Play current selection ──────────────────────────────────────────────── */

static float gen_buf[GFXA_SFXR_PARAM_COUNT];

static void play_sel(void) {
  if ((unsigned)sel < N_GEN) {
    generators[sel](gen_buf);
    gfxa_sfxr_play(gen_buf);
  } else if (sel == IDX_COIN) {
    gfxa_sfxr_play_json(coin_json);
  } else if (sel == IDX_RANDOM) {
    gen_random_params();
    gfxa_sfxr_play(rand_params);
  } else if (sel == IDX_MUTATE) {
    mutate_params();
    gfxa_sfxr_play(rand_params);
  }
}

/* ── App callbacks ───────────────────────────────────────────────────────── */

void gfx_app(int init) {
  if (init) printf("SFXR demo started\n");
  else      printf("SFXR demo stopped\n");
}

void gfx_process_data(int ct) { (void)ct; gfx_delay(1); }

int gfx_draw(float fps) {
  (void)fps;
  gfx_set_color(0, 0, 0);
  gfx_fill_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

  gfx_set_color(0, 200, 255);
  gfx_text("SFXR Sound Effects (jsfxr faithful)", 10, 10, 2);

  gfx_set_color(100,120,140);
  gfx_text("W/S sel, ENTER play, R random, M mutate, 0-9 shortcut, TAB quit", 10, 32, 1);

  int y = 56;
  for (unsigned i = 0; i < N; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%c %s", (int)i == sel ? '>' : ' ', names[i]);
    gfx_set_color((int)i == sel ? 255 : 180, (int)i == sel ? 255 : 180, (int)i == sel ? 100 : 200);
    gfx_text(buf, 10, y, 1);
    y += 14;
  }
  return 1;
}

int gfx_on_key(char key, int down) {
  if (!down) return 0;

  if (key == BTN_GP_UP || key == BTN_KB_UP)    { sel = (sel + N - 1) % N; gfx_beep(400, 15); return 0; }
  if (key == BTN_GP_DOWN || key == BTN_KB_DOWN)  { sel = (sel + 1) % N;    gfx_beep(400, 15); return 0; }

  if (key == BTN_GP_A || key == ' ' || key == '\n' || key == '\r') {
    play_sel();
    return 0;
  }

  if (key == 'r' || key == 'R') {
    sel = IDX_RANDOM;
    gen_random_params();
    gfxa_sfxr_play(rand_params);
    return 0;
  }

  if (key == BTN_GP_B || key == 'm' || key == 'M') {
    if (rand_params[GFXA_SFXR_BASE_FREQ] < 0.01f)
      gen_random_params();
    else
      mutate_params();
    sel = IDX_MUTATE;
    gfxa_sfxr_play(rand_params);
    return 0;
  }

  if (key == '0') { sel = IDX_MUTATE; play_sel(); return 0; }
  if (key >= '1' && key <= '9') {
    int idx = key - '1';
    if (idx < (int)N) { sel = idx; play_sel(); }
    return 0;
  }

  if (key == BTN_GP_MENU) return 1;
  return 0;
}
