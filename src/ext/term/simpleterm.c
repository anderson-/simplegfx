#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "simpleterm.h"
#include "simplegfx.h"

static const char* (*prompt_fn)() = NULL;
static int (*eval_fn)(const char*) = NULL;
static void (*scroll_fn)(const char*) = NULL;
static void (*history_push_fn)(const char*) = NULL;
static const char* (*history_prev_fn)(int) = NULL;

cmd_entry_t gfxt_cmd_registry[MAX_COMMANDS];
int gfxt_cmd_registry_len = 0;

static int width = 40;
static int height = 12;
static int buffer_size = 0;
static int initialized = 0;
static char * buffer = NULL;
static int first_line_end = 0;
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
int cursor_color = 7;
int frame = 0;
int scroll = 0;
static int draw_cursor = 0;
static int history_index = -1;

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
      int code = gfxt_cmd_registry[i].func(args);
      if (code != 0) {
        gfxt_printf(TERM_BRED "\x13%d\n" TERM_RESET, code);
      }
      return code;
    }
  }

  gfxt_printf(TERM_RED "Command not found: %s\n" TERM_RESET, cmd);
  return 1;
}

void prompt() {
  gfxt_printf("%s", prompt_fn());
  input_start = cursor;
  draw_cursor = cursor;
  history_index = -1;
}

void gfxt_init(int w_chars, int h_chars, const char* (*_prompt_fn)(void), int (*_eval_fn)(const char*), void (*_scroll_fn)(const char*), void (*_history_push_fn)(const char*), const char* (*_history_prev_fn)(int)) {
  width = w_chars;
  height = h_chars;
  eval_fn = _eval_fn ? _eval_fn : gfxt_run_cmd;
  prompt_fn = _prompt_fn;
  scroll_fn = _scroll_fn;
  history_push_fn = _history_push_fn;
  history_prev_fn = _history_prev_fn;
  buffer_size = width * height * 2;
  buffer = malloc(buffer_size);
  memset(buffer, 0, buffer_size);
  if (buffer == NULL) return;
  fg_color = default_fg_color;
  bg_color = default_bg_color;
  initialized = 1;
  prompt();
}

void update_xy(char c, int *x, int *y, int check_scroll) {
  if (c == '\n') {
    *x = 0;
    (*y)++;
    if (check_scroll && first_line_end == 0) first_line_end = current_char + 1;
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
  }
  if (check_scroll && ((*x + 1 >= width && *y + 1 >= height) || *y >= height)) {
    scroll = 1;
  }
}

void gfxt_putchar(char c) {
  if (scroll) {
    first_line_end++;
    char end = buffer[first_line_end];
    buffer[first_line_end] = '\0';
    if (scroll_fn) scroll_fn(buffer);
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
    current_char -= first_line_end;
    input_start -= first_line_end;
    if (input_start < 0) input_start = 0;
    scroll = 0;
    ansi_reset(&putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
    { // reset and rescan
      first_line_end = 0;
      putchar_x = 0;
      putchar_y = 0;
      current_char = 0;
      char *p = buffer;
      while (*p) {
        int action = ansi_feed(*p, &putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
        if (action == NON_ANSI_CHAR) {
          update_xy(*p, &putchar_x, &putchar_y, 1);
          current_char = p - buffer;
        } else if (action > 0) {
          ansi_reset(&putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
        }
        p++;
      }
    }
  }
  int action = ansi_feed(c, &putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);

  ansi_debug_char(c);
  if (action == NON_ANSI_CHAR) {
    update_xy(c, &putchar_x, &putchar_y, 1);
    if (c == '\b') {
      if (draw_cursor > input_start) {
        for (int i = draw_cursor; i < cursor; i++) {
          buffer[i - 1] = buffer[i];
        }
        buffer[cursor - 1] = '\0';
        cursor--;
        draw_cursor--;
      }
      return;
    } else if (c == '\x7f') {
      if (draw_cursor < cursor) {
        for (int i = draw_cursor; i < cursor - 1; i++) {
          buffer[i] = buffer[i + 1];
        }
        buffer[cursor - 1] = '\0';
        cursor--;
        draw_cursor--;
      }
      return;
    } else if (c == '\n') {
      if (draw_cursor != cursor) {
        draw_cursor = cursor;
      }
    } else if (c == '\t') {
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
            printf("%s\n", gfxt_cmd_registry[i].name);
            gfxt_printf("  %s", gfxt_cmd_registry[i].name);
          }
        }
        gfxt_printf("\n");
        prompt();
      }
      return;
    }
    if (cursor == draw_cursor) {
      current_char = cursor;
      buffer[cursor] = c;
      cursor++;
      draw_cursor++;
    } else {
      for (int i = cursor; i > draw_cursor; i--) {
        buffer[i] = buffer[i - 1];
      }
      buffer[draw_cursor] = c;
      cursor++;
      draw_cursor++;
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
        if (history_prev_fn) {
          history_index++;
          const char* cmd = history_prev_fn(history_index);
          if (cmd) {
            int len = strlen(cmd);
            memset(buffer + input_start, ' ', cursor - input_start);
            memcpy(buffer + input_start, cmd, len);
            cursor = input_start + len;
            draw_cursor = cursor;
          } else {
            history_index--;
          }
        }
        break;
      case ANSI_CURSOR_DOWN:
        if (history_index > 0) {
          history_index--;
          if (history_prev_fn) {
            const char* cmd = history_prev_fn(history_index);
            if (cmd) {
              int len = strlen(cmd);
              memset(buffer + input_start, ' ', cursor - input_start);
              memcpy(buffer + input_start, cmd, len);
              cursor = input_start + len;
              draw_cursor = cursor;
            }
          }
        } else {
          memset(buffer + input_start, ' ', cursor - input_start);
          history_index = -1;
          cursor = input_start;
          draw_cursor = cursor;
        }
        break;
      case ANSI_CURSOR_RIGHT:
        if (draw_cursor < cursor) draw_cursor++;
        break;
      case ANSI_CURSOR_LEFT:
        if (draw_cursor > input_start) draw_cursor--;
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
    }
    ansi_reset(&putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
  }
  //buffer[cursor] = '\0';
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
  prompt();
}

char gfxt_getchar() {
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

char gfxt_process_char(char c) {
  int action = ansi_feed(c, &ansi_state, &ansi_param_count, ansi_params);
  if (action == NON_ANSI_CHAR) {
    return c;
  } else if (action > 0) {
    if (action == ANSI_COLOR) {
      for (int i = 0; i < ansi_param_count; i++) {
        if (ansi_params[i] == 0) {
          fg_color = default_fg_color;
          bg_color = default_bg_color;
        } else if (ansi_params[i] == 1) {
          // Bold/bright flag - not implemented
        } else if (ansi_params[i] >= 30 && ansi_params[i] <= 37) {
          uint8_t fg = ansi_params[i] - 30;
          fg_color = fg;
        } else if (ansi_params[i] >= 90 && ansi_params[i] <= 97) {
          uint8_t fg = ansi_params[i] - 90 + 8; // Bright colors 8-15
          fg_color = fg;
        } else if (ansi_params[i] >= 40 && ansi_params[i] <= 47) {
          uint8_t bg = ansi_params[i] - 40;
          bg_color = bg;
        } else if (ansi_params[i] >= 100 && ansi_params[i] <= 107) {
          uint8_t bg = ansi_params[i] - 100 + 8; // Bright backgrounds 8-15
          bg_color = bg;
        }
      }
    } else {
      printf("not implemented: %d\n", action);
    }
    ansi_reset(&ansi_state, &ansi_param_count, ansi_params);
  }
  return 0;
}

void gfxt_set_theme(int theme) {
  ansi_set_theme(theme);
}

void gfxt_draw(int x, int y, int size) {
  if (!initialized) return;
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
    c = gfxt_process_char(c);
    int px = x + pos_x * fwidth;
    int py = y + pos_y * fheight;
    if (c) {
      ansi_set_color(bg_color);
      gfx_fill_rect(px, py, fwidth, fheight);
      if (c != ' ' && c != '\n') {
        ansi_set_color(fg_color);
        gfx_draw_char(c, px, py, size, f.data, f.width, f.height);
      }
    }
    if (i >= input_start && i < cursor) {
      ansi_set_color(3);
      int px = x + pos_x * fwidth;
      int py = y + pos_y * fheight;
      gfx_fill_rect(px, py, fwidth, size);
    }
    if (draw_cursor == i && frame % 20 < 10) {
      ansi_set_color(cursor_color);
      int px = x + pos_x * fwidth;
      int py = y + pos_y * fheight;
      gfx_fill_rect(px, py, fwidth, fheight);
    }
    if (c) update_xy(c, &pos_x, &pos_y, 0);
    c = buffer[++i];
    if (!c) break;
  }
  if (draw_cursor == i && frame % 20 < 10) {
    ansi_set_color(cursor_color);
    int px = x + pos_x * fwidth;
    int py = y + pos_y * fheight;
    gfx_fill_rect(px, py, fwidth, fheight);
  }
  frame++;
}

void gfxt_on_key(uint8_t key) {
  if (!initialized) return;

  if (!gfxt_stdin) {
    gfxt_stdin = key;
    return;
  }

  if (key ==  'j') {
    gfxt_clear();
    return;
  }

  gfxt_putchar(key);
  if (key == '\n') {
    char input[256];
    memcpy(input, buffer + input_start, cursor - input_start - 1);
    input[cursor - input_start - 1] = 0;
    if (history_push_fn) history_push_fn(input);
    eval_fn(input);
    prompt();
  }
}
