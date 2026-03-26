#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "simpleterm.h"
#include "simplegfx.h"

#define NON_ANSI_CHAR -1
#define ANSI_NONE 0
#define ANSI_COLOR 1
#define ANSI_CURSOR_UP 2
#define ANSI_CURSOR_DOWN 3
#define ANSI_CURSOR_RIGHT 4
#define ANSI_CURSOR_LEFT 5
#define ANSI_CURSOR_POS 6
#define ANSI_CLEAR_SCREEN 7
#define ANSI_CLEAR_LINE 8
#define ANSI_ERASE_LINE 9

static const char* (*prompt_fn)() = NULL;
static void (*eval_fn)(const char*) = NULL;
static void (*scroll_fn)(const char*) = NULL;

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

void debug_char(char c) {
  if (c >= 32 && c <= 126) {
    printf("%c", c);
  } else if (c == '\n') {
    printf("\n");
  } else {
    printf("[%02X]", c);
  }
}

void test_scroll(const char* msg) {
  printf("SCROLL: |");
  while (*msg) {
    debug_char(*msg);
    msg++;
  }
  printf("|\n");
}

void prompt() {
  gfxt_printf("%s", prompt_fn());
  input_start = cursor;
}

void gfxt_init(int w_chars, int h_chars, void (*_eval_fn)(const char*), const char* (*_prompt_fn)(void)) {
  width = w_chars;
  height = h_chars;
  eval_fn = _eval_fn;
  prompt_fn = _prompt_fn;
  buffer_size = width * height * 2;
  buffer = malloc(buffer_size);
  memset(buffer, 0, buffer_size);
  if (buffer == NULL) return;
  fg_color = default_fg_color;
  bg_color = default_bg_color;
  scroll_fn = test_scroll;
  initialized = 1;
  prompt();
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
          case 'l':
          case 'L':
            action = ANSI_CLEAR_SCREEN;
            break;
        }
        return action;
      }
  }
  return ANSI_NONE;
}

void update_xy(char c, int *x, int *y, int check_scroll) {
  if (c == '\n') {
    *x = 0;
    (*y)++;
    if (check_scroll && first_line_end == 0) first_line_end = current_char + 1;
  } else if (c == '\r') {
    *x = 0;
  } else if (c == '\b') {
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
  if (check_scroll && *x + 1 >= width && *y + 1 >= height) {
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
        }
        p++;
      }
    }
  }
  int action = ansi_feed(c, &putchar_ansi_state, &putchar_ansi_param_count, putchar_ansi_params);
  if (action == NON_ANSI_CHAR) {
    update_xy(c, &putchar_x, &putchar_y, 1);
    current_char = cursor;
  }
  buffer[cursor] = c;
  cursor++;
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
  gfxt_printf("\x1b[l");
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
    update_xy(c, &pos_x, &pos_y, 0);
    return c;
  } else if (action > 0) {
    switch (action) {
      case ANSI_COLOR:
        for (int i = 0; i <= ansi_param_count; i++) {
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
        ansi_reset(&ansi_state, &ansi_param_count, ansi_params);
        break;
      case ANSI_CURSOR_LEFT:
        cursor--;
        break;
      case ANSI_CURSOR_RIGHT:
        cursor++;
        break;
      case ANSI_CURSOR_UP:
        //txt_get_cursor(NULL, &height);
        //txt_set_cursor(-1, height - (ansi_params[0] ? ansi_params[0] : 1));
        break;

      case ANSI_CURSOR_POS:
        //txt_set_cursor(ansi_params[1] - 1, ansi_params[0] - 1);
        break;

      case ANSI_CLEAR_SCREEN:
        //txt_clear();
        break;
    }
  }
  return 0;
}

static uint8_t palette[16][3] = {
  {0x00, 0x00, 0x00}, // Black
  {0xFF, 0x00, 0x00}, // Red
  {0x00, 0xFF, 0x00}, // Green
  {0xFF, 0xFF, 0x00}, // Yellow
  {0x00, 0x00, 0xFF}, // Blue
  {0xFF, 0x00, 0xFF}, // Magenta
  {0x00, 0xFF, 0xFF}, // Cyan
  {0xFF, 0xFF, 0xFF}, // White
  {0x80, 0x80, 0x80}, // Bright Black
  {0xFF, 0x80, 0x80}, // Bright Red
  {0x80, 0xFF, 0x80}, // Bright Green
  {0xFF, 0xFF, 0x80}, // Bright Yellow
  {0x80, 0x80, 0xFF}, // Bright Blue
  {0xFF, 0x80, 0xFF}, // Bright Magenta
  {0x80, 0xFF, 0xFF}, // Bright Cyan
  {0xFF, 0xFF, 0xFF}  // Bright White
};

void set_color(int color) {
  gfx_set_color(palette[color][0], palette[color][1], palette[color][2]);
}

void gfxt_draw(int x, int y, int size) {
  if (!initialized) return;
  pos_x = 0;
  pos_y = 0;
  ansi_reset(&ansi_state, &ansi_param_count, ansi_params);
  font_t f = *gfx_get_font();
  int fheight = f.height;
  int fwidth = f.width;
  int i = 0;
  char c = buffer[i];
  while (1) {
    int px = x + pos_x * (fwidth * size + size);
    int py = y + pos_y * (fheight * size + size);
    c = gfxt_process_char(c);
    if (c) {
      set_color(bg_color);
      gfx_fill_rect(px, py, fwidth * size + size, fheight * size + size);
      if (c != ' ' && c != '\n') {
        char buf[2] = { c, 0 };
        set_color(fg_color);
        gfx_text(buf, px, py, size);
      }
    }
    i++;
    c = buffer[i];
    if (cursor == i && frame % 20 < 10) {
      set_color(cursor_color);
      int px = x + pos_x * (fwidth * size + size);
      int py = y + pos_y * (fheight * size + size);
      gfx_fill_rect(px, py, fwidth * size + size, fheight * size + size);
    }
    if (!c) break;
  }
  frame++;
}

void gfxt_on_key(uint8_t key) {
  if (!initialized) return;

  if (!gfxt_stdin) {
    gfxt_stdin = key;
    return;
  }

  gfxt_putchar(key);
  if (key == '\n') {
    char input[256];
    memcpy(input, buffer + input_start, cursor - input_start - 1);
    input[cursor - input_start - 1] = 0;
    eval_fn(input);
    prompt();
  }
}
