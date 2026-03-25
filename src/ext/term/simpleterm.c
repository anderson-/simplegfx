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
static const char* (*prompt_fn)() = NULL;
static void (*eval_fn)(const char*);
static char input[256] = {0};
static int input_size = 0;

void gfxt_init(int w_chars, int h_chars, void (*_eval_fn)(const char*), const char* (*_prompt_fn)(void)) {
  term_w_chars = w_chars;
  term_h_chars = h_chars;
  eval_fn = _eval_fn;
  prompt_fn = _prompt_fn;

  txt_set_size(w_chars, h_chars);
  txt_set_colors(7, 0);
  txt_clear();

  ansi_reset();
  initialized = 1;
}

void gfxt_putchar(char c) {  
  int action = ansi_feed(c);

  if (action == -1) {
    txt_putc(c);
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

void gfxt_print(const char *str) {
  for (int i = 0; str[i]; i++) {
    gfxt_putchar(str[i]);
  }
}

void gfxt_println(const char *str) {
  gfxt_printf(str);
  gfxt_putchar('\n'); 
}

void gfxt_printf(const char *format, ...) {
  static char temp[256];
  if (!initialized) return;
  va_list args;
  va_start(args, format);
  vsnprintf(temp, sizeof(temp), format, args);
  gfxt_print(temp);
  va_end(args);
}

void gfxt_clear() {
  gfxt_printf("\x1b[l");
}

volatile char gfxt_stdin = 1;

char gfxt_getchar() {
  gfxt_stdin = 0;
  while (gfx_yeld && !gfxt_stdin) {
    gfx_yeld();
  }
  return gfxt_stdin;
}

char* gfxt_gets(char *str, int count) {
  if (!str) return NULL;
  int i = 0;
  char c;
  while ((c = gfxt_getchar()) != '\n' && c != '\r' && i < count) {
    str[i++] = c;
    gfxt_putchar(c);
  }
  str[i] = '\0';
  gfxt_putchar('\n');
  return str;
}

int gfxt_scanf(const char *format, ...) {
  static char buffer[256];
  va_list args;
  int count;
  gfxt_gets(buffer, 255);
  va_start(args, format);
  count = vsscanf(buffer, format, args);
  va_end(args);
  return count;
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

  int cursor_x, cursor_y;
  txt_get_cursor(&cursor_x, &cursor_y);
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

      if (cursor_y == row && cursor_x == col && ++m % 20 < 7) {
        uint8_t bg_r, bg_g, bg_b;
        ansi_color_to_rgb(7, &bg_r, &bg_g, &bg_b);
        gfx_set_color(bg_r, bg_g, bg_b);
        gfx_fill_rect(px, py, fwidth * size + size, fheight * size + size);
      }
    }
  }
}

static int escape_state = 0;

void gfxt_on_key(uint8_t key) {
  if (!initialized) return;

  if (!gfxt_stdin) {
    gfxt_stdin = key;
    return;
  }

  if (escape_state == 1) {
    if (key == '[') {
      escape_state = 2;
      return;
    } else {
      escape_state = 0;
    }
  } else if (escape_state == 2) {
    escape_state = 0;
    switch (key) {
      case 'l':
      case 'L':
        txt_clear();
        return;
      case 'A': // Up
        
        return;
      case 'B': // Down
        
        return;
      case 'C': // Right
        {
          int cursor_x, cursor_y;
          txt_get_cursor(&cursor_x, &cursor_y);
          txt_set_cursor(cursor_x + 1, cursor_y);
        }
        return;
      case 'D': // Left
        {
          int cursor_x, cursor_y;
          txt_get_cursor(&cursor_x, &cursor_y);
          txt_set_cursor(cursor_x - 1, cursor_y);
        }
        return;
    }
  }

  if (key == '\x1b') {
    escape_state = 1;
    return;
  }

  gfxt_putchar(key);
  input[input_size++] = key;
  if (key == '\n') {
    input[input_size - 1] = 0;
    eval_fn(input);
    input[0] = 0;
    input_size = 0;
    gfxt_printf("%s", prompt_fn());
  }
}
