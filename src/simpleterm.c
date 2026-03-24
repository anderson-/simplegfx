#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "simpleterm.h"
#include "simplegfx.h"
#include "simpletext.h"
#include "simplerepl.h"
#include "simpleansi.h"

#define FONT_W 5
#define FONT_H 7

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
              txt_set_color(7, 0);
            } else if (params[i] >= 30 && params[i] <= 37) {
              uint8_t fg = params[i] - 30;
              txt_set_color(fg, -1);
            } else if (params[i] >= 40 && params[i] <= 47) {
              uint8_t bg = params[i] - 40;
              txt_set_color(-1, bg);
            }
          }
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

void gfxt_draw(int x, int y, int size) {
  if (!initialized) return;

  int w, h;
  txt_get_size(&w, &h);

  char *text_buf = txt_get_text_buffer();
  uint8_t *fg_buf = txt_get_fg_buffer();
  uint8_t *bg_buf = txt_get_bg_buffer();

  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      int pos = row * w + col;
      char c = text_buf[pos];
      uint8_t fg = fg_buf[pos];
      uint8_t bg = bg_buf[pos];

      int px = x + col * FONT_W * size;
      int py = y + row * FONT_H * size;

      uint8_t bg_r, bg_g, bg_b;
      ansi_color_to_rgb(bg + 40, &bg_r, &bg_g, &bg_b);
      gfx_set_color(bg_r, bg_g, bg_b);
      gfx_fill_rect(px, py, FONT_W * size, FONT_H * size);

      if (c != ' ') {
        uint8_t fg_r, fg_g, fg_b;
        ansi_color_to_rgb(fg + 30, &fg_r, &fg_g, &fg_b);
        gfx_set_color(fg_r, fg_g, fg_b);

        char buf[2] = { c, 0 };
        gfx_text(buf, px, py, size);
      }
    }
  }

  const char *prompt = repl_get_prompt();
  const char *input = repl_get_input();
  int cursor = repl_get_cursor();

  int input_y = y + h * FONT_H * size;
  int input_x = x;

  gfx_set_color(20, 20, 35);
  gfx_fill_rect(x, input_y, w * FONT_W * size, FONT_H * size);

  gfx_set_color(192, 192, 192);
  gfx_text(prompt, input_x, input_y, size);
  input_x += strlen(prompt) * FONT_W * size;

  for (int i = 0; input[i]; i++) {
    char buf[2] = { input[i], 0 };
    if (i == cursor) {
      gfx_set_color(255, 255, 255);
      gfx_fill_rect(input_x, input_y, FONT_W * size, FONT_H * size);
      gfx_set_color(0, 0, 0);
    } else {
      gfx_set_color(192, 192, 192);
    }
    gfx_text(buf, input_x, input_y, size);
    input_x += FONT_W * size;
  }

  if (cursor == strlen(input)) {
    gfx_set_color(255, 255, 255);
    gfx_fill_rect(input_x, input_y, FONT_W * size, FONT_H * size);
  }
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
