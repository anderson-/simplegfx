#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simplegfx.h"
#include "ext/term/simpleterm.h"
#include "ext/term/stdcmds.h"

#define HISTORY_SIZE 50

static char* history[HISTORY_SIZE];
static int history_count = 0;
static int history_start = 0;

void history_push_fn(const char* cmd) {
  if (!cmd || strlen(cmd) == 0) return;

  if (history_count > 0) {
    int last_idx = (history_start + history_count - 1) % HISTORY_SIZE;
    if (strcmp(history[last_idx], cmd) == 0) return;
  }

  if (history_count >= HISTORY_SIZE) {
    free(history[history_start]);
    history_start = (history_start + 1) % HISTORY_SIZE;
    history_count--;
  }

  int idx = (history_start + history_count) % HISTORY_SIZE;
  history[idx] = strdup(cmd);
  history_count++;
}

char* history_prev_fn(int index) {
  if (index < 0 || index >= history_count) return NULL;
  return history[(history_start + history_count - 1 - index) % HISTORY_SIZE];
}

char* get_prompt(void) {
  return "\x1b[32msimplegfx\x1b[m:\x1b[34m~\x1b[m$ ";
}

void gfx_app(int init) {
  int fsize = 3;
  int fw, fh;
  gfx_get_font_size(&fw, &fh, fsize);
  int w = WINDOW_WIDTH / fw;
  int h = WINDOW_HEIGHT / fh;
  int x = (WINDOW_WIDTH - w * fw) / 2;
  int y = (WINDOW_HEIGHT - h * fh) / 2;
  gfxt_set_prompt_handler(get_prompt);
  gfxt_set_history_handler(history_push_fn, history_prev_fn);
  gfxt_std_cmd_reg();
  gfxt_init(w, h);
  gfxt_set_drawing_params(x, y, fsize);
}

int gfx_draw(float fps) {
  return gfxt_draw();
}

char last_key = 0;

#define KEY_CTRL -32
#define KEY_ENTER 13
#define KEY_BACKSPACE 8
#define KEY_DELETE 127

int gfx_on_key(char key, int down) {
  if (!down) {
    last_key = 0;
    return 0;
  }

#ifdef DEBUG
  printf("[key:%d]\n", key);
#endif

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
    case KEY_BACKSPACE:
      gfxt_on_key('\b');
      return 0;
    case KEY_DELETE:
      gfxt_on_key('\x7f');
      return 0;
  }

  if (key == last_key) {
    return 0;
  }

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
