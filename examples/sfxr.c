#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simplegfx.h"
#include "sfxr.h"
#include "sfxr_presets.h"

/* ── Geradores via biblioteca ────────────────────────────────────────────── */

#define IDX_RANDOM  (GFXA_SFXR_PRESET_COUNT)   /* 8 */
#define IDX_MUTATE  9
#define N           10

static const char *names[] = {
  "Pickup/Coin", "Laser/Shoot", "Explosion", "Power-up",
  "Hit/Hurt", "Jump", "Blip/Select",
  "Coin", "Random", "Mutate"
};

static float rand_params[GFXA_SFXR_PARAM_COUNT];
static int sel = 0;

/* ── Toca som original e depois a versão quantizada ─────────────────────── */

static void play_both(const float p[GFXA_SFXR_PARAM_COUNT]) {
  gfxa_sfxr_play(p);
  float q[GFXA_SFXR_PARAM_COUNT];
  char b64[32];
  gfxa_sfxr_to_base64(p, b64, sizeof(b64));
  gfxa_sfxr_from_base64(b64, q);
  gfxa_sfxr_play(q);
}

static void play_sel(void) {
  float buf[GFXA_SFXR_PARAM_COUNT];
  if ((unsigned)sel < GFXA_SFXR_PRESET_COUNT) {
    gfxa_sfxr_preset(sel, buf);
    play_both(buf);
  } else if (sel == IDX_RANDOM) {
    gfxa_sfxr_random(rand_params);
    play_both(rand_params);
  } else if (sel == IDX_MUTATE) {
    if (rand_params[GFXA_SFXR_BASE_FREQ] < 0.01f)
      gfxa_sfxr_random(rand_params);
    else
      gfxa_sfxr_mutate(rand_params);
    play_both(rand_params);
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
    gfxa_sfxr_random(rand_params);
    play_both(rand_params);
    return 0;
  }

  if (key == BTN_GP_B || key == 'm' || key == 'M') {
    if (rand_params[GFXA_SFXR_BASE_FREQ] < 0.01f)
      gfxa_sfxr_random(rand_params);
    else
      gfxa_sfxr_mutate(rand_params);
    sel = IDX_MUTATE;
    play_both(rand_params);
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
