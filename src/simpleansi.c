#include <stdint.h>
#include "simpleansi.h"

#define ANSI_NONE 0
#define ANSI_COLOR 1
#define ANSI_CURSOR_UP 2
#define ANSI_CURSOR_DOWN 3
#define ANSI_CURSOR_RIGHT 4
#define ANSI_CURSOR_LEFT 5
#define ANSI_CURSOR_POS 6
#define ANSI_CLEAR_SCREEN 7
#define ANSI_CLEAR_LINE 8

static int ansi_state = 0;
static int ansi_params[8];
static int ansi_param_count = 0;

void ansi_reset(void) {
  ansi_state = 0;
  ansi_param_count = 0;
  for (int i = 0; i < 8; i++) ansi_params[i] = 0;
}

int ansi_feed(char c) {
  switch (ansi_state) {
    case 0:
      if (c == '\x1b') {
        ansi_state = 1;
        return ANSI_NONE;
      }
      return -1;

    case 1:
      if (c == '[') {
        ansi_state = 2;
        ansi_param_count = 0;
        for (int i = 0; i < 8; i++) ansi_params[i] = 0;
        return ANSI_NONE;
      }
      ansi_reset();
      return -1;

    case 2:
      if (c >= '0' && c <= '9') {
        if (ansi_param_count < 8) {
          ansi_params[ansi_param_count] = ansi_params[ansi_param_count] * 10 + (c - '0');
        }
        return ANSI_NONE;
      } else if (c == ';') {
        ansi_param_count++;
        return ANSI_NONE;
      } else {
        int action = ANSI_NONE;
        switch (c) {
          case 'm':
            action = ANSI_COLOR;
            break;
          case 'A':
            action = ANSI_CURSOR_UP;
            break;
          case 'B':
            action = ANSI_CURSOR_DOWN;
            break;
          case 'C':
            action = ANSI_CURSOR_RIGHT;
            break;
          case 'D':
            action = ANSI_CURSOR_LEFT;
            break;
          case 'H':
            action = ANSI_CURSOR_POS;
            break;
          case 'J':
            if (ansi_params[0] == 2) action = ANSI_CLEAR_SCREEN;
            break;
          case 'K':
            action = ANSI_CLEAR_LINE;
            break;
        }
        return action;
      }
  }
  return ANSI_NONE;
}

int* ansi_get_params(void) {
  return ansi_params;
}

int ansi_get_param_count(void) {
  return ansi_param_count + 1;
}

void ansi_color_to_rgb(int code, uint8_t *r, uint8_t *g, uint8_t *b) {
  // Cores normais (0-7) e bright (8-15)
  static const uint8_t colors[16][3] = {
    {0, 0, 0},       // 0: black
    {128, 0, 0},     // 1: red
    {0, 128, 0},     // 2: green
    {128, 128, 0},   // 3: yellow
    {0, 0, 128},     // 4: blue
    {128, 0, 128},   // 5: magenta
    {0, 128, 128},   // 6: cyan
    {192, 192, 192}, // 7: white
    {64, 64, 64},    // 8: bright black (gray)
    {255, 0, 0},     // 9: bright red
    {0, 255, 0},     // 10: bright green
    {255, 255, 0},   // 11: bright yellow
    {0, 0, 255},     // 12: bright blue
    {255, 0, 255},   // 13: bright magenta
    {0, 255, 255},   // 14: bright cyan
    {255, 255, 255}  // 15: bright white
  };

  if (code >= 0 && code <= 15) {
    if (r) *r = colors[code][0];
    if (g) *g = colors[code][1];
    if (b) *b = colors[code][2];
  } else if (code >= 30 && code <= 37) {
    code -= 30; // 30-37 → 0-7
    if (r) *r = colors[code][0];
    if (g) *g = colors[code][1];
    if (b) *b = colors[code][2];
  } else if (code >= 40 && code <= 47) {
    code -= 40; // 40-47 → 0-7
    if (r) *r = colors[code][0];
    if (g) *g = colors[code][1];
    if (b) *b = colors[code][2];
  } else if (code == 0) {
    if (r) *r = 192;
    if (g) *g = 192;
    if (b) *b = 192;
  }
}
