#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simplegfx.h>
#include <rg35xx_btns.h>

#define RECT_COUNT 1000

char last_key = 0;
char text[32] = {0};
char menu = 0;

void render(void) {
  for (int i = 0; i < RECT_COUNT; i++) {
    gfx_set_color(rand() % 10, rand() % (menu ? 60 : 35), rand() % 40);
    gfx_fill_rect(rand() % WINDOW_WIDTH, rand() % WINDOW_HEIGHT, rand() % 50 + 10, rand() % 50 + 10);
  }

  gfx_set_color(255, 255, 255);
  gfx_text("\xae Press MENU + X to exit \xaf", 10, 10, 2);

  int y;
  y = gfx_font_table(10, 50, 1);
  y = gfx_font_table(10, y + 10, 2);
  y = gfx_font_table(10, y + 10, 3);

  if (last_key != 0) {
    sprintf(text, "key: %d", last_key);
    gfx_text(text, 400, 10, 2);
  }
}

int on_key(char key, int down) {
  if (down) {
    printf("Key pressed: %d\n", key);
    if (key == BTN_MENU) {
      menu = 1;
    }
    if (menu && key == BTN_X) {
      return 1;
    }
    last_key = key;
  } else {
    printf("Key released: %d\n", key);
    if (key == BTN_MENU) {
      menu = 0;
    }
    last_key = 0;
  }
  return 0;
}