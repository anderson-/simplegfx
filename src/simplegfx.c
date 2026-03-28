#include "simplegfx.h"

static font_t * _font = NULL;

uint32_t elm = 0;
char * printf_buf = NULL;
int printf_size = 0;
int printf_len = 0;
int full_kb = 0;
int spacing = 1;

void (* gfx_yeld)() = NULL;

void gfx_set_font(font_t * font) {
  _font = font;
  if (_font == NULL) {
    _font = &font5x7;
  }
  if (_font->count == 0) {
    uint8_t * data = _font->data;
    for (int i = 0; i < 512 * _font->width; i++) {
      if (data[i] == 1 && data[i + 1] == 2
        && data[i + 2] == 3 && data[i + 3] == 4
        && data[i + 4] == 5) {
        _font->count = i / _font->width;
        break;
      }
    }
  }
}

font_t * gfx_get_font(void) {
  return _font;
}

inline void gfx_draw_char(char c, int x, int y, int size, uint8_t * font_data, int font_width, int font_height) {
  y += font_height * size;
  for (int cx = 0; cx < font_width; cx++) {
    for (int cy = 1; cy <= font_height; cy++) {
      uint8_t mask = 1 << (font_height - cy);
      if (font_data[(uint8_t)c * font_width + cx] & mask) {
        if (size == 1) {
          gfx_point(x + cx, y - cy);
        } else {
          gfx_fill_rect(x + cx * size, y - cy * size, size, size);
        }
      }
    }
  }
}

int gfx_text(const char * text, int x, int y, int size) {
  if (text == NULL || _font == NULL) {
    return y;
  }
  int len = (int)strlen(text);
  font_t f = *_font;
  int fheight = f.height;
  int fwidth = f.width;
  int cx = x;
  int cy = y;
  for (int i = 0; i < len; i++) {
    uint8_t C = (uint8_t)text[i];
    if (C == '\r' && text[i + 1] == '\n') {
      i++;
      cx = x;
      cy += fheight * size + spacing * size;
      if (cy > WINDOW_HEIGHT) {
        cy = y;
      }
      continue;
    }
    gfx_draw_char(C, cx, cy, size, f.data, fwidth, fheight);
    cx += fwidth * size + spacing * size;
    if (cx > WINDOW_WIDTH) {
      cx = x;
      cy += fheight * size + spacing * size;
    }
  }
  cy += fheight * size + spacing * size;
  return cy;
}

int gfx_clear_text_buffer(void) {
  if (printf_buf != NULL) {
    free(printf_buf);
    printf_buf = NULL;
  }
  printf_size = 0;
  printf_len = 0;
  return 0;
}

int gfx_printf(const char * format, ...) {
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len < 0) {
    return -1;
  }
  if (len + printf_len >= printf_size) {
    printf_size = (len + printf_len + 64) * 2;
    char * new_buf = (char *)realloc(printf_buf, printf_size);
    if (new_buf == NULL) {
      return -1;
    }
    printf_buf = new_buf;
  }
  va_start(args, format);
  vsnprintf(printf_buf + printf_len, printf_size - printf_len, format, args);
  va_end(args);
  printf_len += len;
  return len;
}

int gfx_font_table(int x, int y, int size) {
  if (_font == NULL) return y;
  font_t f = *_font;
  char t[2] = {0, 0};
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 32; j++) {
      t[0] = i * 32 + j;
      if ((uint8_t)t[0] >= f.count) break;
      gfx_draw_char(t[0], x + j * (f.width * size + spacing * size),
                    y + i * (f.height * size + spacing * size), size, f.data, f.width, f.height);
    }
  }
  return y + 8 * (f.height * size + spacing * size);
}

unsigned int _seed = 12345;
int gfx_fast_rand(void) {
  _seed = _seed * 1103515245 + 12345;
  return (unsigned int)(_seed / 65536) % 32768;
}