#include "ansiutils.h"
#include <stdint.h>
#include <stdio.h>

static uint16_t current_theme = 0;

void ansi_set_theme(int theme) {
  if (theme < 0) {
    ansi_set_theme((current_theme + 1) % NUM_THEMES);
    return;
  }
  if (theme >= (int) NUM_THEMES) return;
  current_theme = theme;
  for (int i = 0; i < 16; i++) {
    uint16_t c = themes[theme][i];
    palette[i][0] = (c >> 11) << 3;
    palette[i][1] = ((c >> 5) & 0x3F) << 2;
    palette[i][2] = (c & 0x1F) << 3;
  }
}

void ansi_set_color(int color) {
  gfx_set_color(palette[color][0], palette[color][1], palette[color][2]);
}

void ansi_reset(int *state, int *param_count, int *params) {
  *state = 0;
  *param_count = 0;
  for (int i = 0; i < 8; i++) params[i] = 0;
}

int ansi_feed(char c, int *state, int *param_count, int *params) {
  switch (*state) {
    case 0:
      if (c == '\x1b') {
        *state = 1;
        return ANSI_NONE;
      }
      return NON_ANSI_CHAR;
    case 1:
      if (c == '[') {
        *state = 2;
        *param_count = 0;
        for (int i = 0; i < 8; i++) params[i] = 0;
        return ANSI_NONE;
      }
      ansi_reset(state, param_count, params);
      return NON_ANSI_CHAR;
    case 2:
      if (c >= '0' && c <= '9') {
        if (*param_count < 8) {
          params[*param_count] = params[*param_count] * 10 + (c - '0');
        }
        return ANSI_NONE;
      } else if (c == ';') {
        (*param_count)++;
        return ANSI_NONE;
      } else {
        int action = ANSI_NONE;
        (*param_count)++;
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
            if (params[0] == 2) action = ANSI_CLEAR_SCREEN;
            break;
          case 'K':
            action = ANSI_CLEAR_LINE;
            break;
          case 'S':
            action = ANSI_SCROLL_UP;
            break;
          case 'T':
            action = ANSI_SCROLL_DOWN;
            break;
          default:
            printf("unknown %d\n", c);
            ansi_reset(state, param_count, params);
            break;
        }
        return action;
      }
  }
  return ANSI_NONE;
}

void ansi_debug_char(char c) {
  if (c >= 32 && c <= 126) {
    printf("%c", c);
  } else if (c == '\n') {
    printf("\n");
  } else {
    printf("[%02X]", c);
  }
}
