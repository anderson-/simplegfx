#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simplegfx.h>
#include <rg35xx_btns.h>

#define RECT_COUNT 1000

char last_key = 0;
char text[32] = {0};
char menu = 0;
extern uint32_t elm;

void render(int freetime) {
  for (int i = 0; i < RECT_COUNT; i++) {
    gfx_set_color(rand() % 30 + 10, rand() % (menu ? 60 : 30) + 10, rand() % 90 + 10);
    gfx_fill_rect(rand() % WINDOW_WIDTH, rand() % WINDOW_HEIGHT, rand() % 50 + 10, rand() % 50 + 10);
  }

  gfx_set_color(255, 255, 255);
  gfx_text("\xae Press MENU + X to exit \xaf", 10, 10, 2);

  int y;
  gfx_set_color(255, 0, 255);
  y = gfx_font_table(10, 50, 1);
  gfx_set_color(255, 255, 0);
  y = gfx_font_table(10, y + 10, 2);
  gfx_set_color(0, 255, 255);
  y = gfx_font_table(10, y + 10, 3);

  if (last_key != 0) {
    sprintf(text, "key: %d", last_key);
    gfx_text(text, 350, 10, 2);
  }
  sprintf(text, "free: %d ms", freetime);
  gfx_text(text, 500, 10, 1);
  sprintf(text, "elm: %d", elm);
  gfx_text(text, 500, 20, 1);
}

int on_key(char key, int down) {
  if (down) {
    printf("Key pressed: %d\n", key);
    beep(261, 50);
    if (key == BTN_MENU) {
      menu = 1;
    }
    if (menu && key == BTN_X) {
      return 1;
    }
    last_key = key;
  } else {
    printf("Key released: %d\n", key);
    beep(392, 50);
    if (key == BTN_MENU) {
      menu = 0;
    }
    last_key = 0;
  }
  return 0;
}