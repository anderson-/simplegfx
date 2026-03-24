#include <stdint.h>
#include <string.h>
#include "simpletext.h"

#define MAX_TERM_W 80
#define MAX_TERM_H 24

static char text_buf[MAX_TERM_W * MAX_TERM_H];
static uint8_t fg_buf[MAX_TERM_W * MAX_TERM_H];
static uint8_t bg_buf[MAX_TERM_W * MAX_TERM_H];

static int term_w = 40;
static int term_h = 10;
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t default_fg = 7;
static uint8_t default_bg = 0;
static uint8_t current_fg = 7;
static uint8_t current_bg = 0;

void txt_set_size(int w, int h) {
  if (w <= MAX_TERM_W && h <= MAX_TERM_H) {
    term_w = w;
    term_h = h;
    txt_clear();
  }
}

void txt_get_size(int *w, int *h) {
  if (w) *w = term_w;
  if (h) *h = term_h;
}

void txt_set_colors(int8_t fg, int8_t bg) {
  if (fg != -1) default_fg = fg;
  if (bg != -1) default_bg = bg;
  current_fg = default_fg;
  current_bg = default_bg;
}

void txt_reset_colors(void) {
  current_fg = default_fg;
  current_bg = default_bg;
}

void txt_set_cursor(int x, int y) {
  if (x >= 0 && x < term_w && y >= 0 && y < term_h) {
    cursor_x = x;
    cursor_y = y;
  }
}

void txt_shift_cursor(int sx, int sy) {
  cursor_x += sx;
  cursor_y += sy;
  if (cursor_x < 0) cursor_x = 0;
  if (cursor_y < 0) cursor_y = 0;
  if (cursor_x >= term_w) cursor_x = term_w - 1;
  if (cursor_y >= term_h) cursor_y = term_h - 1;
}

void txt_get_cursor(int *x, int *y) {
  if (x) *x = cursor_x;
  if (y) *y = cursor_y;
}

void txt_clear(void) {
  memset(text_buf, ' ', term_w * term_h);
  memset(fg_buf, default_fg, term_w * term_h);
  memset(bg_buf, default_bg, term_w * term_h);
  cursor_x = 0;
  cursor_y = 0;
  current_fg = default_fg;
  current_bg = default_bg;
}

void txt_putc(char c) {
  int pos = cursor_y * term_w + cursor_x;

  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (c == '\r') {
    cursor_x = 0;
  } else if (c == '\t') {
    cursor_x = (cursor_x + 8) & ~7;
  } else {
    if (pos < term_w * term_h) {
      text_buf[pos] = c;
      fg_buf[pos] = current_fg;
      bg_buf[pos] = current_bg;
      cursor_x++;
    }
  }

  if (cursor_x >= term_w) {
    cursor_x = 0;
    cursor_y++;
  }

  if (cursor_y >= term_h) {
    txt_scroll(1);
    cursor_y = term_h - 1;
  }
}

void txt_write(const char *s) {
  while (*s) {
    txt_putc(*s++);
  }
}

void txt_scroll(int lines) {
  if (lines <= 0 || lines >= term_h) {
    txt_clear();
    return;
  }

  int move_cells = (term_h - lines) * term_w;
  memmove(text_buf, text_buf + lines * term_w, move_cells);
  memmove(fg_buf, fg_buf + lines * term_w, move_cells);
  memmove(bg_buf, bg_buf + lines * term_w, move_cells);

  int clear_start = (term_h - lines) * term_w;
  int clear_count = lines * term_w;
  memset(text_buf + clear_start, ' ', clear_count);
  memset(fg_buf + clear_start, default_fg, clear_count);
  memset(bg_buf + clear_start, default_bg, clear_count);
}

char* txt_get_text_buffer(void) {
  return text_buf;
}

uint8_t* txt_get_fg_buffer(void) {
  return fg_buf;
}

uint8_t* txt_get_bg_buffer(void) {
  return bg_buf;
}

void txt_set_color(int8_t fg, int8_t bg) {
  if (fg != -1) current_fg = fg;
  if (bg != -1) current_bg = bg;
}

void txt_get_color(int8_t *fg, int8_t *bg) {
  if (fg) *fg = current_fg;
  if (bg) *bg = current_bg;
}
