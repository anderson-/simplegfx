#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "simpleterm.h"
#include "ansiutils.h"
#include "simplegfx.h"

static char* (*prompt_fn)() = NULL;
static int (*eval_fn)(const char*) = NULL;
static void (*scroll_push_fn)(const char*, int) = NULL;
static char* (*scroll_prev_fn)(int) = NULL;
static void (*history_push_fn)(const char*) = NULL;
static char* (*history_prev_fn)(int) = NULL;

cmd_entry_t gfxt_cmd_registry[MAX_COMMANDS];
int gfxt_cmd_registry_len = 0;

static const char spinner[] = "\xdc\xdd\xdf\xde";
static int width = 40;
static int height = 12;
static int buffer_size = 0;
static int initialized = 0;
static char * buffer = NULL;
static int first_line_end = 0;
static int last_line_start = 0;
static int input_start = 0;
static int cursor = 0;
static int current_char = 0;
static int ansi_state = 0;
static int ansi_params[8];
static int ansi_param_count = 0;
static int putchar_ansi_state = 0;
static int putchar_ansi_params[8];
static int putchar_ansi_param_count = 0;
static int putchar_x = 0;
static int putchar_y = 0;
static int default_fg_color = 7;
static int default_bg_color = 0;
static int fg_color = 0;
static int bg_color = 0;
static int pos_x = 0;
static int pos_y = 0;
volatile char gfxt_stdin = 1;
int gfxt_stdin_state = 0;
int cursor_color = 7;
int frame = 0;
int scroll = 0;
static int draw_cursor = 0;
static int history_index = -1;
static int busy = 0;
static int busy_cursor = 0;
static int change = 0;
static int key_input_process = 0;
static int pager_lines = 0;
static int pager_quit = 0;
static int pager_waiting = 0;

void gfxt_register_cmd(const char* name, const char* help, int (*func)(const char*)) {
  if (gfxt_cmd_registry_len < MAX_COMMANDS) {
    gfxt_cmd_registry[gfxt_cmd_registry_len] = (cmd_entry_t){name, func, help};
    gfxt_cmd_registry_len++;
  }
}

int gfxt_run_cmd(const char* line) {
  char cmd[32] = {0};
  char args[128] = {0};
  if (sscanf(line, "%31s %127[^\n]", cmd, args) < 1) {
    return 1;
  }
  for (int i = 0; i < gfxt_cmd_registry_len; i++) {
    if (strcmp(gfxt_cmd_registry[i].name, cmd) == 0) {
      pager_lines = 0;
      pager_quit = 0;
      busy = 1;
      int code = gfxt_cmd_registry[i].func(args);
      pager_lines = 0;
      pager_quit = 0;
      if (code != 0) {
        gfxt_printf(TERM_BRED "\x13%d\n" TERM_RESET, code);
      }
      return code;
    }
  }
  gfxt_printf(TERM_RED "%s: not found\n" TERM_RESET, cmd);
  return 1;
}

void gfxt_set_history_handler(void (*_history_push_fn)(const char*), char* (*_history_prev_fn)(int)) {
  history_push_fn = _history_push_fn;
  history_prev_fn = _history_prev_fn;
}

void gfxt_set_scroll_handler(void (*_scroll_push_fn)(const char*, int), char* (*_scroll_prev_fn)(int)) {
  scroll_push_fn = _scroll_push_fn;
  scroll_prev_fn = _scroll_prev_fn;
}

void gfxt_set_eval_handler(int (*_eval_fn)(const char*)) {
  eval_fn = _eval_fn ? _eval_fn : gfxt_run_cmd;
}

void gfxt_set_prompt_handler(char* (*_prompt_fn)(void)) {
  prompt_fn = _prompt_fn;
}

static void _prompt() {
  if (prompt_fn) {
    gfxt_printf("\x1b[m%s", prompt_fn());
  } else {
    gfxt_printf("\x1b[m> ");
  }
  input_start = cursor;
  draw_cursor = cursor;
  history_index = -1;
  busy = 0;
}

void gfxt_init(int w_chars, int h_chars) {
  if (buffer != NULL) {
    free(buffer);
    buffer = NULL;
  }
  if (w_chars <= 0 || h_chars <= 0) {
    initialized = 0;
    return;
  }
  width = w_chars;
  height = h_chars;
  eval_fn = eval_fn ? eval_fn : gfxt_run_cmd;
  buffer_size = width * height * 2;
  buffer = malloc(buffer_size);
  if (buffer == NULL) return;
  memset(buffer, 0, buffer_size);
  fg_color = default_fg_color;
  bg_color = default_bg_color;
  initialized = 1;
  _prompt();
}

void _update_xy(char c, int *x, int *y, int check_scroll) {
  if (c == '\n') {
    *x = 0;
    (*y)++;
    if (check_scroll && first_line_end == 0) first_line_end = current_char + 1;
    if (check_scroll) last_line_start = current_char + 1;
  } else if (c == '\r') {
    *x = 0;
  } else if (c == '\b' || c == '\x7f') {
    (*x)--;
  } else {
    (*x)++;
  }
  if (*x < 0) *x = 0;
  if (*y < 0) *y = 0;
  if (*x >= width) {
    *x = 0;
    (*y)++;
    if (check_scroll && first_line_end == 0) first_line_end = current_char + 1;
    if (check_scroll) last_line_start = current_char + 1;
  }
  if (check_scroll && ((*x + 1 >= width && *y + 1 >= height) || *y >= height)) {
    scroll = 1;
  }
}

void _autocomplete() {
  if (cursor == input_start) {
    return;
  }
  int prefix = 0;
  int id = -1;
  for (int i = 0; i < gfxt_cmd_registry_len; i++) {
    if (strncmp(buffer + input_start, gfxt_cmd_registry[i].name, strlen(buffer + input_start)) == 0) {
      prefix++;
      id = i;
    }
  }
  if (prefix == 1) {
    memset(buffer + cursor, ' ', cursor - input_start);
    memcpy(buffer + input_start, gfxt_cmd_registry[id].name, strlen(gfxt_cmd_registry[id].name));
    cursor = input_start + strlen(gfxt_cmd_registry[id].name);
    draw_cursor = cursor;
  } else if (prefix > 1) {
    gfxt_printf("\n");
    int start = input_start;
    int len = strlen(buffer + start);
    for (int i = 0; i < gfxt_cmd_registry_len; i++) {
      if (memcmp(buffer + start, gfxt_cmd_registry[i].name, len - 1) == 0) {
#ifdef DEBUG
        printf("%s\n", gfxt_cmd_registry[i].name);
#endif
        gfxt_printf("  %s", gfxt_cmd_registry[i].name);
      }
    }
    gfxt_printf("\n");
    _prompt();
    char * p = buffer + start;
    while (*p && *p != '\n') {
      gfxt_putchar(*p);
      p++;
    }
  }
}

static inline void _check_resize() {
  if (cursor + 64 >= buffer_size) {
  buffer_size += 64;
  buffer = realloc(buffer, buffer_size);
  if (buffer == NULL) return;
#ifdef DEBUG
    printf("Buffer resized to %d\n", buffer_size);
#endif
  }
}

void _scroll_line() {
  first_line_end++;
  char end = buffer[first_line_end];
  buffer[first_line_end] = '\0';
  if (scroll_push_fn) scroll_push_fn(buffer, -1);
  buffer[first_line_end] = end;
  char * src = buffer + first_line_end;
  char * dst = buffer;
  while (*src) {
    *dst = *src;
    src++;
    dst++;
  }
  memset(dst, 0, buffer_size - (dst - buffer));
  cursor -= first_line_end;
  draw_cursor -= first_line_end;
  current_char -= first_line_end;
  input_start -= first_line_end;
  last_line_start -= first_line_end;
  if (input_start < 0) input_start = 0;
  if (last_line_start < 0) last_line_start = 0;
  scroll = 0;
  ansi_reset(&putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
  { // reset and rescan
    first_line_end = 0;
    last_line_start = 0;
    putchar_x = 0;
    putchar_y = 0;
    current_char = 0;
    char *p = buffer;
    while (*p) {
      int action = ansi_feed(*p, &putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
      if (action == NON_ANSI_CHAR) {
        _update_xy(*p, &putchar_x, &putchar_y, 1);
        current_char = p - buffer;
      } else if (action > 0) {
        ansi_reset(&putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
      }
      p++;
    }
  }
}

static inline void _backspace_at_cursor() {
  if (draw_cursor > input_start) {
    for (int i = draw_cursor; i < cursor; i++) {
      buffer[i - 1] = buffer[i];
    }
    buffer[cursor - 1] = '\0';
    cursor--;
    draw_cursor--;
  }
}

static inline void _del_at_cursor() {
  if (draw_cursor < cursor) {
    for (int i = draw_cursor; i < cursor - 1; i++) {
      buffer[i] = buffer[i + 1];
    }
    buffer[cursor - 1] = '\0';
    cursor--;
    draw_cursor--;
  }
}

void _append_at_cursor(char c) {
  for (int i = cursor; i > draw_cursor; i--) {
    buffer[i] = buffer[i - 1];
  }
  buffer[draw_cursor] = c;
  cursor++;
  draw_cursor++;
}

void _fetch_history(int direction) {
  if (direction > 0) {
    if (history_prev_fn) {
      history_index++;
      const char* cmd = history_prev_fn(history_index);
      if (cmd) {
        int len = strlen(cmd);
        memset(buffer + input_start, '\0', cursor - input_start);
        memcpy(buffer + input_start, cmd, len);
        cursor = input_start + len;
        draw_cursor = cursor;
      } else {
        history_index--;
      }
    }
  } else {
    if (history_index > 0) {
      history_index--;
      if (history_prev_fn) {
        const char* cmd = history_prev_fn(history_index);
        if (cmd) {
          int len = strlen(cmd);
          memset(buffer + input_start, '\0', cursor - input_start);
          memcpy(buffer + input_start, cmd, len);
          cursor = input_start + len;
          draw_cursor = cursor;
        }
      }
    } else {
      memset(buffer + input_start, '\0', cursor - input_start);
      history_index = -1;
      cursor = input_start;
      draw_cursor = cursor;
    }
  }
}

void gfxt_putchar(char c) {
  change = 2;
  _check_resize();
  if (scroll) {
    _scroll_line();
  }
  int action = ansi_feed(c, &putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);

#ifdef TERM_OUTPUT_STDOUT
  printf("%c", c);
#elif defined(DEBUG)
  ansi_debug_char(c);
#endif

  if (action == NON_ANSI_CHAR) {
    _update_xy(c, &putchar_x, &putchar_y, 1);
    if (c == '\b') {
      _backspace_at_cursor();
      return;
    } else if (c == '\x7f') {
      _del_at_cursor();
      return;
    } else if (c == '\n') {
      if (draw_cursor != cursor) {
        draw_cursor = cursor;
      }
      busy_cursor = cursor;
      current_char = cursor;
      if (key_input_process && history_push_fn) history_push_fn(buffer + input_start);
      if (cursor > 0) {
        buffer[cursor] = c;
        cursor++;
        draw_cursor++;
      }
      if (key_input_process) {
        key_input_process = 0;
        eval_fn(buffer + input_start);
        _prompt();
      } else if (!pager_quit) {
        pager_lines++;
        if (pager_lines >= height - 2) {
          pager_lines = 0;
          pager_waiting = 1;
          change = 2;
          char k = gfxt_getchar();
          pager_waiting = 0;
          if (k == 'q' || k == 'Q') {
            pager_quit = 1;
          }
        }
      }
      return;
    } else if (c == '\t') {
      key_input_process = 0;
      _autocomplete();
      return;
    }
    if (cursor == draw_cursor) {
      current_char = cursor;
      buffer[cursor] = c;
      cursor++;
      draw_cursor++;
    } else {
      _append_at_cursor(c);
    }
  } else if (action > 0) {
    switch (action) {
      case ANSI_COLOR:
        {
          int shift = cursor;
          buffer[cursor++] = '\x1b';
          buffer[cursor++] = '[';
          for (int i = 0; i < putchar_ansi_param_count; i++) {
            cursor += sprintf(buffer + cursor, "%d", putchar_ansi_params[i]);
            if (i < putchar_ansi_param_count - 1) {
              buffer[cursor++] = ';';
            }
          }
          buffer[cursor++] = 'm';
          shift = cursor - shift;
          draw_cursor += shift;
        }
        break;
      case ANSI_CURSOR_UP:
        if (key_input_process) {
          _fetch_history(1);
        }
        break;
      case ANSI_CURSOR_DOWN:
        if (key_input_process) {
          _fetch_history(-1);
        }
        break;
      case ANSI_CURSOR_RIGHT:
        if (key_input_process) {
          if (draw_cursor < cursor) draw_cursor++;
        }
        break;
      case ANSI_CURSOR_LEFT:
        if (key_input_process) {
          if (draw_cursor > input_start) draw_cursor--;
        }
        break;
      case ANSI_CURSOR_POS:
        // Handle cursor position
        break;
      case ANSI_CLEAR_SCREEN:
        switch (putchar_ansi_params[0]) {
          case 2:
            memset(buffer, 0, buffer_size);
            cursor = 0;
            draw_cursor = 0;
            input_start = 0;
            first_line_end = 0;
            last_line_start = 0;
            putchar_x = 0;
            putchar_y = 0;
            current_char = 0;
            break;
        }
        break;
      case ANSI_CLEAR_LINE:
        switch (putchar_ansi_params[0]) {
          case 0:
            memset(buffer + draw_cursor, ' ', cursor - draw_cursor);
            break;
          case 1:
            memset(buffer + input_start, ' ', draw_cursor - input_start);
            break;
          case 2:
            memset(buffer + input_start, ' ', first_line_end - input_start);
            break;
        }
        break;
      case ANSI_SCROLL_UP:
        if (key_input_process) {
          // Handle scroll up - lines added at the bottom
        }
        break;
      case ANSI_SCROLL_DOWN:
        if (key_input_process) {
          // Handle scroll down - lines added at the top
        }
        break;
    }
    ansi_reset(&putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
  }
}

int gfxt_print(const char *str) {
  int i = 0;
  while (str[i]) {
    gfxt_putchar(str[i]);
    i++;
  }
  return i;
}

int gfxt_println(const char *str) {
  int len = gfxt_print(str);
  gfxt_putchar('\n');
  return len + 1;
}

int gfxt_printf(const char *format, ...) {
  static char temp[256];
  if (!initialized) return 0;
  va_list args;
  va_start(args, format);
  vsnprintf(temp, sizeof(temp), format, args);
  int len = gfxt_print(temp);
  va_end(args);
  return len;
}

void gfxt_clear() {
  gfxt_printf("\x1b[2J");
}

char gfxt_getchar() {
  busy = 0;
  gfxt_stdin = 0;
  while (gfx_yeld && !gfxt_stdin) {
    gfx_yeld();
  }
  return gfxt_stdin;
}

char* gfxt_gets(char *str, int count) {
  if (!str) return NULL;
  int i = 0;
  char c;
  while ((c = gfxt_getchar()) != '\n' && c != '\r' && i < count) {
    str[i++] = c;
    gfxt_putchar(c);
  }
  str[i] = '\0';
  gfxt_putchar('\n');
  return str;
}

int gfxt_scanf(const char *format, ...) {
  static char buffer[256];
  va_list args;
  int count;
  gfxt_gets(buffer, 255);
  va_start(args, format);
  count = vsscanf(buffer, format, args);
  va_end(args);
  return count;
}

char _process_ansi(char c) {
  int action = ansi_feed(c, &ansi_state, &ansi_param_count, ansi_params);
  if (action == NON_ANSI_CHAR) {
    return c;
  } else if (action > 0) {
    if (action == ANSI_COLOR) {
      for (int i = 0; i < ansi_param_count; i++) {
        if (ansi_params[i] == 0) {
          fg_color = default_fg_color;
          bg_color = default_bg_color;
        } else if (ansi_params[i] >= 30 && ansi_params[i] <= 37) {
          uint8_t fg = ansi_params[i] - 30;
          fg_color = fg;
        } else if (ansi_params[i] >= 90 && ansi_params[i] <= 97) {
          uint8_t fg = ansi_params[i] - 90 + 8;
          fg_color = fg;
        } else if (ansi_params[i] >= 40 && ansi_params[i] <= 47) {
          uint8_t bg = ansi_params[i] - 40;
          bg_color = bg;
        } else if (ansi_params[i] >= 100 && ansi_params[i] <= 107) {
          uint8_t bg = ansi_params[i] - 100 + 8;
          bg_color = bg;
        }
      }
    } else {
#ifdef DEBUG
      printf("[not implemented: %d]\n", action);
#endif
    }
    ansi_reset(&ansi_state, &ansi_param_count, ansi_params);
  }
  return 0;
}

void gfxt_set_theme(int theme) {
  ansi_set_theme(theme);
}

int gfxt_draw(int x, int y, int size) {
  if (!initialized) return 0;
  frame++;
  if (busy) busy++;
  if (frame % 10 == 1) {
    change = 2;
  } else if (busy > 5) {
    change = 2;
  }
  if (!change) {
    return 0;
  }
  pos_x = 0;
  pos_y = 0;
  ansi_reset(&ansi_state, &ansi_param_count, ansi_params);
  font_t f = *gfx_get_font();
  int fheight = (f.height + spacing) * size;
  int fwidth = (f.width + spacing) * size;
  ansi_set_color(bg_color);
  gfx_fill_rect(x, y, fwidth * width, fheight * height);
  int i = 0;
  char c = buffer[i];
  while (1) {
    c = _process_ansi(c);
    int px = x + pos_x * fwidth;
    int py = y + pos_y * fheight;
    if (c) {
      ansi_set_color(bg_color);
      gfx_fill_rect(px, py, fwidth, fheight);
      if (c != ' ' && c != '\n') {
        ansi_set_color(fg_color);
        if (busy > 5 && i >= input_start && i < cursor) {
          ansi_set_color(8);
        }
        gfx_draw_char(c, px, py, size, f.data, f.width, f.height);
      }
    }
#ifdef DEBUG
    if (i >= input_start && i < cursor) {
      ansi_set_color(3);
      int px = x + pos_x * fwidth;
      int py = y + pos_y * fheight;
      gfx_fill_rect(px, py, fwidth, size);
    }
    if (i >= 0 && i < first_line_end) {
      ansi_set_color(4);
      int px = x + pos_x * fwidth;
      int py = y + pos_y * fheight;
      gfx_fill_rect(px, py + size, fwidth, size);
    }
    if (i > last_line_start && i < cursor) {
      ansi_set_color(5);
      int px = x + pos_x * fwidth;
      int py = y + pos_y * fheight;
      gfx_fill_rect(px, py + size * 2, fwidth, size);
    }
#endif
    if (busy > 5 && busy_cursor == i) {
      int spin_idx = (frame/2) % 4;
      char spin_char = spinner[spin_idx];
      ansi_set_color(8);
      gfx_draw_char(spin_char, x + pos_x * fwidth, y + pos_y * fheight, size, f.data, f.width, f.height);
    } else if (draw_cursor == i && frame % 20 < 10) {
      int px = x + pos_x * fwidth;
      int py = y + pos_y * fheight;
      gfx_fill_rect(px, py, fwidth, fheight);
    }
    if (c) _update_xy(c, &pos_x, &pos_y, 0);
    c = buffer[++i];
    if (!c) break;
  }
  if (pager_waiting) {
    static const char* more_str = "--- more (q quit) ---";
    int py = y + (height - 1) * fheight;
    ansi_set_color(8);
    gfx_fill_rect(x, py, fwidth * width, fheight);
    ansi_set_color(0);
    int mx = x;
    for (const char *p = more_str; *p; p++) {
      gfx_draw_char(*p, mx, py + size, size, f.data, f.width, f.height);
      mx += fwidth;
    }
  } else if (!busy && draw_cursor == i && frame % 20 < 10) {
    ansi_set_color(cursor_color);
    int px = x + pos_x * fwidth;
    int py = y + pos_y * fheight;
    gfx_fill_rect(px, py, fwidth, fheight);
  }
  change--;
  return 1;
}

void gfxt_on_key(uint8_t key) {
  if (!initialized) return;
  change = 1;

  if (!gfxt_stdin) {
    if (key == '\x1b' && gfxt_stdin_state == 0) {
      gfxt_stdin_state = 1;
      return;
    } else if (gfxt_stdin_state == 1) {
      if (key == '[') {
        gfxt_stdin_state = 2;
        return;
      }
      gfxt_stdin_state = 0;
    } else if (gfxt_stdin_state == 2) {
      switch (key) {
        case 'A':
          gfxt_stdin = ANSI_CURSOR_UP;
          break;
        case 'B':
          gfxt_stdin = ANSI_CURSOR_DOWN;
          break;
        case 'C':
          gfxt_stdin = ANSI_CURSOR_RIGHT;
          break;
        case 'D':
          gfxt_stdin = ANSI_CURSOR_LEFT;
          break;
      }
      gfxt_stdin_state = 0;
      return;
    }
    gfxt_stdin = key;
    return;
  }

  key_input_process = 1;
  gfxt_putchar(key);
  key_input_process = 0;
}

void gfxt_set_busy(int _busy) {
  busy = _busy;
}

void gfxt_get_size(int *w, int *h) {
  if (w) *w = width;
  if (h) *h = height;
}

#ifdef TEST
char * get_buffer(void) {
  return buffer;
}
#endif