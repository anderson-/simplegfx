#include <stdio.h>
#include <stdlib.h>
#include <simplegfx.h>

static void draw_matrix() {
  static int columns[80] = {0};
  static int initialized = 0;

  if (!initialized) {
    for (int i = 0; i < 80; i++) {
      columns[i] = rand() % WINDOW_HEIGHT;
    }
    initialized = 1;
  }

  gfx_set_color(0, 10, 0);
  for (int y = 0; y < WINDOW_HEIGHT; y += 20) {
    for (int x = 0; x < WINDOW_WIDTH; x += 20) {
      gfx_fill_rect(x, y, 20, 20);
    }
  }

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
        char ch[2] = {(char)(rand() % 94 + 33), 0};
        gfx_text(ch, x, y, 1);
      }
    }
  }
}

void gfx_app(int init) {}

void gfx_process_data(int compute_time) {}

void gfx_draw(float fps) {
  gfx_set_color(0, 0, 0);
  gfx_fill_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  draw_matrix();
}

int gfx_on_key(char key, int down) {
  if (key == BTN_MENU) {
    return 1;
  }

  return 0;
}
