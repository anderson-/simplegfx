/* sprite — animated sprite viewer / benchmark
   Iterates over characters and actions on key press.
   Build: make sdl-sprite  (or sdl-sprite-run to launch) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simplegfx.h"
#include "coin.h"
#include "spr_draw.h"

/* Sprite is 48×48.  Pick a scale that fits the desktop window (640×480). */
#define SPR_SCALE   4
#define SPR_W       (48 * SPR_SCALE)
#define SPR_H       (48 * SPR_SCALE)
#define SPR_CX      ((WINDOW_WIDTH  - SPR_W) / 2)
#define SPR_CY      ((WINDOW_HEIGHT - SPR_H) / 2)

/* Bench — 8192 extras = 64 kB of positions */
#define MAX_EXTRA   8192

static int      cur_ch    = 0;
static int      cur_act   = 0;
static int      cur_t     = 0;
static int      paused    = 0;

/* Extra sprites — flat arrays, no struct */
static int      extra_x[MAX_EXTRA];
static int      extra_y[MAX_EXTRA];
static int      nextra    = 0;

/* Palette override */
static uint8_t  rand_pal[32 * 3];
static int      use_rand_pal = 0;

/* ── helpers ──────────────────────────────────────────────────────────────── */

static int nacts(int ch) {
    return spr_offs[ch + 1] - spr_offs[ch];
}

static int ncol(void) {
    uint32_t off = spr_idx[spr_offs[cur_ch] + cur_act];
    return spr_data[off + 3];
}

static void randomize_palette(void) {
    int nc = ncol();
    if (nc > 32) nc = 32;
    for (int i = 0; i < nc * 3; i++)
        rand_pal[i] = gfx_fast_rand() & 0xFF;
    use_rand_pal = 1;
}

/* ── callbacks ────────────────────────────────────────────────────────────── */

void gfx_app(int init) {
    if (init) {
        printf("Sprite bench  (%d char(s))\n", SPR_CHAR_COUNT);
    } else {
        printf("Sprite bench stopped\n");
    }
}

void gfx_process_data(int compute_time) {
    (void)compute_time;
}

int gfx_draw(float fps) {
    char text[80];

    //gfx_set_color(0, 0, 0);
    //gfx_fill_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    const uint8_t *pal = use_rand_pal ? rand_pal : NULL;

    /* draw extra sprites */
    for (int i = 0; i < nextra; i++) {
        spr_draw(spr_data, spr_idx, spr_offs,
                 cur_ch, cur_act,
                 extra_x[i], extra_y[i], cur_t,
                 SPR_SCALE, pal);
    }

    /* draw main sprite centered */
    spr_draw(spr_data, spr_idx, spr_offs, cur_ch, cur_act,
             SPR_CX, SPR_CY, paused ? cur_t : cur_t++/2,
             SPR_SCALE, pal);

    /* info overlay */
    gfx_set_color(255, 255, 0);
    snprintf(text, sizeof(text),
             "Char %d/%d  Act %d/%d  Frame %d  %d%s%s",
             cur_ch, SPR_CHAR_COUNT, cur_act, nacts(cur_ch), cur_t,
             nextra, paused ? "  PAUSED" : "",
             use_rand_pal ? "  [PAL]" : "");
    gfx_text(text, 8, 8, 1);

    gfx_set_color(0, 255, 255);
    snprintf(text, sizeof(text), "%.0f fps", fps);
    gfx_text(text, WINDOW_WIDTH - 52, 8, 1);

    gfx_set_color(100, 100, 100);
    gfx_text(
        "[SPACE] nxtAct  [B] prvAct  [X] nxtCh  [Y] add  [SELECT] clr  [START] pal  [P] pause  [MENU] exit",
        8, WINDOW_HEIGHT - 14, 1);

    return 1;
}

int gfx_on_key(char key, int down) {
    if (!down) return 0;

    if (key == BTN_GP_MENU || key == BTN_GP_POWER_ESC)
        return 1;

    if (key == BTN_GP_A || key == ' ') {
        int n = nacts(cur_ch);
        cur_act = (cur_act + 1) % n;
        cur_t   = 0;
    }

    if (key == BTN_GP_B) {
        int n = nacts(cur_ch);
        cur_act = (cur_act + n - 1) % n;
        cur_t   = 0;
    }

    if (key == BTN_GP_X) {
        cur_ch  = (cur_ch + 1) % SPR_CHAR_COUNT;
        cur_act = 0;
        cur_t   = 0;
    }

    /* Y — spawn at random position */
    if (key == BTN_GP_Y && nextra < MAX_EXTRA) {
        extra_x[nextra] = rand() % (WINDOW_WIDTH  - SPR_W + 1);
        extra_y[nextra] = rand() % (WINDOW_HEIGHT - SPR_H + 1);
        nextra++;
    }

    /* SELECT — clear all */
    if (key == BTN_GP_SELECT) nextra = 0;

    /* START — random palette */
    if (key == BTN_GP_START) randomize_palette();

    if (key == 'p' || key == 'P') paused = !paused;

    return 0;
}
