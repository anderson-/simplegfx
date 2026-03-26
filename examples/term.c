#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simplegfx.h"
#include "ext/term/simpleterm.h"
#include "ext/term/stdcmds.h"

const char* get_prompt(void) {
  return "\x1b[32msimplegfx\x1b[0m:\x1b[34m~\x1b[0m$ ";
}

int x = 0;
int y = 0;
int fsize = 4;

void gfx_app(int init) {
  font_t * f = gfx_get_font();
  int fw = (f->width + 1) * fsize;
  int fh = (f->height + 1) * fsize;
  int w = WINDOW_WIDTH / fw;
  int h = WINDOW_HEIGHT / fh;
  x = (WINDOW_WIDTH - w * fw) / 2;
  y = (WINDOW_HEIGHT - h * fh) / 2;
  gfxt_init(w, h, get_prompt, NULL, NULL, NULL, NULL);
  gfxt_std_cmd_reg();
}

void gfx_draw(float fps) {
  gfxt_draw(x, y, fsize);

  // Show FPS in corner
  gfx_set_color(100, 100, 100);
  char fps_text[32];
  sprintf(fps_text, "%.1f fps", fps);
  gfx_text(fps_text, 2, 2, 1);
}

char last_key = 0;

#define KEY_CTRL -32
#define KEY_ENTER 13

int gfx_on_key(char key, int down) {

  if (!down) {
    last_key = 0;
    return 0;
  }

  switch (key) {
    case BTN_UP:
      gfxt_on_key('\x1b');
      gfxt_on_key('[');
      gfxt_on_key('A');
      return 0;
    case BTN_DOWN:
      gfxt_on_key('\x1b');
      gfxt_on_key('[');
      gfxt_on_key('B');
      return 0;
    case BTN_LEFT:
      gfxt_on_key('\x1b');
      gfxt_on_key('[');
      gfxt_on_key('D');
      return 0;
    case BTN_RIGHT:
      gfxt_on_key('\x1b');
      gfxt_on_key('[');
      gfxt_on_key('C');
      return 0;
    case KEY_CTRL:
      gfxt_on_key('\x1b');
      gfxt_on_key('[');
      return 0;
    case KEY_ENTER:
      gfxt_on_key('\n');
      return 0;
  }

  if (key == last_key) {
    return 0;
  }

  // Pass key to terminal
  gfxt_on_key(key);

  // Exit on MENU+X
  static int menu_pressed = 0;
  if (key == BTN_MENU) {
    menu_pressed = down ? 1 : 0;
  }
  if (menu_pressed && key == BTN_X && down) {
    return 1;
  }

  last_key = key;
  return 0;
}

void gfx_process_data(int compute_time) {}
