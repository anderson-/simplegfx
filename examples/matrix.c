#include <stdio.h>
#include <stdlib.h>
#include "simplegfx.h"

static int columns[80] = {0};
static font_t font;

void gfx_app(int init) {
  font = *gfx_get_font();
  for (int i = 0; i < 80; i++) {
    columns[i] = rand() % WINDOW_HEIGHT;
  }
}

static void draw_matrix() {
  for (int col = 0; col < 80; col++) {
    int x = col * 8;
    columns[col] += rand() % 3 + 1;
    if (columns[col] > WINDOW_HEIGHT + 100) {
      columns[col] = -(rand() % 200);
    }

    for (int i = 0; i < 15; i++) {
      int y = columns[col] - i * 12;
      if (y >= 0 && y < WINDOW_HEIGHT) {
        int brightness = 255 - i * 15;
        if (i == 0) {
          gfx_set_color(200, 255, 200);
        } else {
          gfx_set_color(0, brightness, 0);
        }
        gfx_draw_char(rand() % 94 + 33, x, y, 1, font.data, font.width, font.height);
      }
    }
  }
}

void gfx_process_data(int compute_time) {}

int gfx_draw(float fps) {
  draw_matrix();
  return 1;
}

int gfx_on_key(char key, int down) {
  if (key == BTN_MENU) {
    return 1;
  }

  return 0;
}
