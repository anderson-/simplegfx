#pragma once
#include "ansiutils.h"
#include "simpleterm.h"

static inline void gfxt_sb_draw(char *(*cb)(char *buf, int len), int bg) {
  int tx, ty, fs; gfxt_get_drawing_params(&tx, &ty, &fs);
  int fw, fh; gfx_get_font_size(&fw, &fh, fs);
  int x = tx, y = ty - fh;
  char buf[128];

  ansi_set_color(bg);
  gfx_fill_rect(x, y - fs, (WINDOW_WIDTH / fw) * fw, fh);

  buf[0] = '\0';
  if (cb) cb(buf, sizeof(buf));

  int st = 0, params[8], pc = 0;
  ansi_reset(&st, &pc, params);
  int fg = 7, px = x; bg = 0;

  for (const char *p = buf; *p && px + fw <= x + WINDOW_WIDTH; p++) {
    int act = ansi_feed(*p, &st, &pc, params);
    if (act == NON_ANSI_CHAR) {
      ansi_set_color(bg);
      gfx_fill_rect(px, y - fs, fw, fh);
      ansi_set_color(fg);
      gfx_draw_char(*p, px, y, fs);
      px += fw;
    } else if (act == ANSI_COLOR) {
      for (int j = 0; j < pc; j++) {
        int v = params[j];
        if (v == 0) { fg = 7; bg = 0; }
        else if (v >= 30 && v <= 37) fg = v - 30;
        else if (v >= 90 && v <= 97) fg = v - 90 + 8;
        else if (v >= 40 && v <= 47) bg = v - 40;
        else if (v >= 100 && v <= 107) bg = v - 100 + 8;
      }
      ansi_reset(&st, &pc, params);
    } else if (act != ANSI_NONE) {
      ansi_reset(&st, &pc, params);
    }
  }
}
