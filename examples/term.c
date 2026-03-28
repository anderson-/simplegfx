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

  // Don't add duplicates to the most recent command
  if (history_count > 0) {
    int last_idx = (history_start + history_count - 1) % HISTORY_SIZE;
    if (strcmp(history[last_idx], cmd) == 0) return;
  }

  // Free memory if we're overwriting an old entry
  if (history_count >= HISTORY_SIZE) {
    free(history[history_start]);
    history_start = (history_start + 1) % HISTORY_SIZE;
    history_count--;
  }

  // Add new command to history
  int idx = (history_start + history_count) % HISTORY_SIZE;
  history[idx] = strdup(cmd);
  history_count++;
}

const char* history_prev_fn(int index) {
  printf("history_prev_fn called with index: %d, history_count: %d\n", index, history_count);
  if (index < 0 || index >= history_count) {
    printf("returning NULL - index out of range\n");
    return NULL;
  }

  // Index 0 = most recent, Index 1 = second most recent, etc.
  int idx = (history_start + history_count - 1 - index) % HISTORY_SIZE;
  printf("calculated idx: %d, history_start: %d\n", idx, history_start);
  printf("history[idx] pointer: %p\n", (void*)history[idx]);
  printf("returning history[%d]: '%s'\n", idx, history[idx] ? history[idx] : "NULL");
  return history[idx];
}

static char* up_scroll_history[HISTORY_SIZE];
static int up_scroll_history_count = 0;
static int up_scroll_history_start = 0;
static char* down_scroll_history[HISTORY_SIZE];
static int down_scroll_history_count = 0;
static int down_scroll_history_start = 0;

// direction=-1: line scrolled off the top (stored for scroll-down retrieval)
// direction=+1: line scrolled off the bottom (stored in down history)
void scroll_fn(const char* line, int direction) {
  if (!line || strlen(line) == 0) return;
  if (direction <= 0) {
    // Scrolled off the top → store in up_scroll_history
    if (up_scroll_history_count >= HISTORY_SIZE) {
      free(up_scroll_history[up_scroll_history_start]);
      up_scroll_history_start = (up_scroll_history_start + 1) % HISTORY_SIZE;
      up_scroll_history_count--;
    }
    int idx = (up_scroll_history_start + up_scroll_history_count) % HISTORY_SIZE;
    up_scroll_history[idx] = strdup(line);
    up_scroll_history_count++;
  } else {
    // Scrolled off the bottom → store in down_scroll_history
    if (down_scroll_history_count >= HISTORY_SIZE) {
      free(down_scroll_history[down_scroll_history_start]);
      down_scroll_history_start = (down_scroll_history_start + 1) % HISTORY_SIZE;
      down_scroll_history_count--;
    }
    int idx = (down_scroll_history_start + down_scroll_history_count) % HISTORY_SIZE;
    down_scroll_history[idx] = strdup(line);
    down_scroll_history_count++;
  }
}

// index=0 → most recently pushed line (off top), index=1 → second most recent, etc.
const char* scroll_prev_fn(int index) {
  if (index < 0 || index >= up_scroll_history_count) return NULL;
  int idx = (up_scroll_history_start + up_scroll_history_count - 1 - index) % HISTORY_SIZE;
  return up_scroll_history[idx];
}

// index=0 → oldest pushed line (off bottom), matches scroll_bottom_depth indexing
const char* scroll_next_fn(int index) {
  if (index < 0 || index >= down_scroll_history_count) return NULL;
  int idx = (down_scroll_history_start + index) % HISTORY_SIZE;
  return down_scroll_history[idx];
}






const char* get_prompt(void) {
  return "\x1b[32msimplegfx\x1b[m:\x1b[34m~\x1b[m$ ";
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
  gfxt_init(w, h, get_prompt, NULL, scroll_fn, scroll_prev_fn, scroll_next_fn, history_push_fn, history_prev_fn);
  gfxt_std_cmd_reg();
}

int gfx_draw(float fps) {
  if(!gfxt_draw(x, y, fsize)) {
    return 0;
  }


  // Show FPS in corner
  gfx_set_color(100, 100, 100);
  char fps_text[32];
  sprintf(fps_text, "%.1f fps", fps);
  gfx_text(fps_text, 2, 2, 1);

  return 1;
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

  printf("%d\n", key);

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
