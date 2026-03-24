#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "simpleterm.h"
#include "simplegfx.h"
#include "simpletext.h"
#include "simplerepl.h"
#include "simpleansi.h"

static int term_w_chars = 40;
static int term_h_chars = 12;
static int initialized = 0;

static void default_eval(const char *line) {
  gfxt_printf("Command: %s\n", line);
}

static const char* default_prompt(void) {
  return "user@cardputer$ ";
}

void gfxt_init(int w_chars, int h_chars, void (*eval_fn)(const char*), const char* (*prompt_fn)(void)) {
  term_w_chars = w_chars;
  term_h_chars = h_chars;

  txt_set_size(w_chars, h_chars);
  txt_set_colors(7, 0);
  txt_clear();

  repl_init(eval_fn ? eval_fn : default_eval, prompt_fn ? prompt_fn : default_prompt);

  ansi_reset();
  initialized = 1;
}

void gfxt_printf(const char *format, ...) {
  if (!initialized) return;

  va_list args;
  va_start(args, format);

  char temp[256];
  vsnprintf(temp, sizeof(temp), format, args);

  for (int i = 0; temp[i]; i++) {
    int action = ansi_feed(temp[i]);

    if (action == -1) {
      txt_putc(temp[i]);
    } else if (action > 0) {
      int *params = ansi_get_params();
      int param_count = ansi_get_param_count();

      switch (action) {
        case 1: // ANSI_COLOR
          for (int i = 0; i < param_count; i++) {
            if (params[i] == 0) {
              txt_reset_colors();
            } else if (params[i] == 1) {
              // Bold/bright flag - not implemented separately
            } else if (params[i] >= 30 && params[i] <= 37) {
              uint8_t fg = params[i] - 30;
              txt_set_color(fg, -1);
            } else if (params[i] >= 90 && params[i] <= 97) {
              uint8_t fg = params[i] - 90 + 8; // Bright colors 8-15
              txt_set_color(fg, -1);
            } else if (params[i] >= 40 && params[i] <= 47) {
              uint8_t bg = params[i] - 40;
              txt_set_color(-1, bg);
            } else if (params[i] >= 100 && params[i] <= 107) {
              uint8_t bg = params[i] - 100 + 8; // Bright backgrounds 8-15
              txt_set_color(-1, bg);
            }
          }
          ansi_reset();
          break;

        case 2: // ANSI_CURSOR_UP
          txt_get_cursor(NULL, &term_h_chars);
          txt_set_cursor(-1, term_h_chars - (params[0] ? params[0] : 1));
          break;

        case 6: // ANSI_CURSOR_POS
          txt_set_cursor(params[1] - 1, params[0] - 1);
          break;

        case 7: // ANSI_CLEAR_SCREEN
          txt_clear();
          break;
      }
    }
  }

  va_end(args);
}

int m = 0;

void gfxt_draw(int x, int y, int size) {
  if (!initialized) return;

  int w, h;
  txt_get_size(&w, &h);

  char *text_buf = txt_get_text_buffer();
  uint8_t *fg_buf = txt_get_fg_buffer();
  uint8_t *bg_buf = txt_get_bg_buffer();

  font_t *current_font = gfx_get_font();
  font_t f = *current_font;
  int fheight = f.height;
  int fwidth = f.width;

  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      int pos = row * w + col;
      char c = text_buf[pos];
      uint8_t fg = fg_buf[pos];
      uint8_t bg = bg_buf[pos];

      int px = x + col * (fwidth * size + size);
      int py = y + row * (fheight * size + size);

      uint8_t bg_r, bg_g, bg_b;
      ansi_color_to_rgb(bg, &bg_r, &bg_g, &bg_b);
      gfx_set_color(bg_r, bg_g, bg_b);
      gfx_fill_rect(px, py, fwidth * size + size, fheight * size + size);

      if (c != ' ') {
        uint8_t fg_r, fg_g, fg_b;
        ansi_color_to_rgb(fg, &fg_r, &fg_g, &fg_b);
        gfx_set_color(fg_r, fg_g, fg_b);

        char buf[2] = { c, 0 };
        gfx_text(buf, px, py, size);
      }
    }
  }

  int saved_cursor_x, saved_cursor_y;
  txt_get_cursor(&saved_cursor_x, &saved_cursor_y);

  txt_set_cursor(0, h - 1);
  for (int i = 0; i < w-1; i++) {
    gfxt_printf(" ");
  }

  txt_set_cursor(0, h - 1);
  gfxt_printf("%s", repl_get_prompt());

  int cursor = 0;
  txt_get_cursor(&cursor, NULL);
  cursor += repl_get_cursor();

  gfxt_printf("%s", repl_get_input());

  txt_set_cursor(cursor, h - 1);
  gfxt_printf("\x1b[%dm%c\x1b[0m", (++m % 20 < 7) ? 40 : 47, text_buf[cursor + (h - 1) * w]);

  txt_set_cursor(saved_cursor_x, saved_cursor_y);
}

void gfxt_on_key(uint8_t key, uint8_t modifiers) {
  if (!initialized) return;

  if (modifiers & 2) { // Ctrl
    if (key == 'l' || key == 'L') {
      txt_clear();
      return;
    }
    if (key == 'c' || key == 'C') {
      repl_clear();
      return;
    }
  }

  repl_handle_key(key, modifiers);
}
