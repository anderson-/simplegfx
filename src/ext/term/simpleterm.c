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
static int default_fg_color = 7;
static int default_bg_color = 0;
static int fg_color = 0;
static int bg_color = 0;
static int pos_x = 0;
static int pos_y = 0;
volatile char gfxt_stdin = 1;
static int counter = 0;
int cursor_color = 7;
int frame = 0;
int draw_index = 0;

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
  //prompt();
  for (int j = 0; j < 52; j++) {
    for (int i = 0; i < 11; i++) {
      buffer[j * 11 + i] = (i % 95) + 49;
      cursor++;
    }
  }
}

void gfxt_putchar(char c) {
  buffer[cursor] = c;
  cursor++;
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

void ansi_reset(void) {
  ansi_state = 0;
  ansi_param_count = 0;
  for (int i = 0; i < 8; i++) ansi_params[i] = 0;
}

int ansi_feed(char c) {
  switch (ansi_state) {
    case 0:
      if (c == '\x1b') {
        ansi_state = 1;
        return ANSI_NONE;
      }
      return -1;
    case 1:
      if (c == '[') {
        ansi_state = 2;
        ansi_param_count = 0;
        for (int i = 0; i < 8; i++) ansi_params[i] = 0;
        return ANSI_NONE;
      }
      ansi_reset();
      return -1;
    case 2:
      if (c >= '0' && c <= '9') {
        if (ansi_param_count < 8) {
          ansi_params[ansi_param_count] = ansi_params[ansi_param_count] * 10 + (c - '0');
        }
        return ANSI_NONE;
      } else if (c == ';') {
        ansi_param_count++;
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
            if (ansi_params[0] == 2) action = ANSI_CLEAR_SCREEN;
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

int exits = 0;

void update_cursor(char c) {
  if (pos_y >= height) {
    char end = buffer[first_line_end + 1];
    buffer[first_line_end + 1] = '\0';
    if (scroll_fn) scroll_fn(buffer);
    buffer[first_line_end + 1] = end;
    printf("pre scrolling %c - frame %d\n", end, frame);

    for (int i = 0; buffer[i] != '\0'; i++) {
      debug_char(buffer[i]);
    }
    printf("\n");


    int i = 0;
    char * src = buffer + first_line_end + 1;
    char * dst = buffer;
    printf("src: %c\n", *src);
    printf("dst: %c\n", *dst);

    while (*src) {
      *dst = *src;
      src++;
      dst++;
    }
    *dst = '\0';

    printf("scrolling\n");

    for (int i = 0; buffer[i] != '\0'; i++) {
      debug_char(buffer[i]);
    }
    printf("\n");

    exits++;

    cursor -= first_line_end + 1;
    pos_x = 0;
    pos_y = height - 1; // TODO: scroll
  }
  if (c == '\n') {
    pos_x = 0;
    pos_y++;
    if (first_line_end == 0) first_line_end = current_char + 1;
  } else if (c == '\r') {
    pos_x = 0;
  } else if (c == '\b') {
    pos_x--;
  } else {
    pos_x++;
  }
  if (pos_x < 0) pos_x = 0;
  if (pos_y < 0) pos_y = 0;
  if (pos_x >= width) {
    pos_x = 0;
    pos_y++;
    if (first_line_end == 0) first_line_end = current_char + 1;
  }
  if (exits) {
    return;
  }
}

char gfxt_process_char(char c) {
  int action = ansi_feed(c);
  if (action == NON_ANSI_CHAR) {
    update_cursor(c);
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
        ansi_reset();
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
  size = 1;
  frame++;
  if (!initialized) return;
  pos_x = 0;
  pos_y = 0;
  first_line_end = 0;
  current_char = 0;
  font_t f = *gfx_get_font();
  int fheight = f.height;
  int fwidth = f.width;
  int draw_index = 0;
  char c = buffer[draw_index];
  while (c) {
    int px = x + pos_x * (fwidth * size + size);
    int py = y + pos_y * (fheight * size + size);
    c = gfxt_process_char(c);
    if (c) {
      current_char = draw_index;
      if (!first_line_end) {
        set_color(default_bg_color + 2);
      } else {
        set_color(bg_color);
      }
      gfx_fill_rect(px, py, fwidth * size + size, fheight * size + size);
      if (c != ' ' && c != '\n') {
        char buf[2] = { c, 0 };
        set_color(fg_color);
        gfx_text(buf, px, py, size);
      }
      if (cursor == draw_index && ++counter % 20 < 10) {
        set_color(cursor_color);
        gfx_fill_rect(px, py, fwidth * size + size, fheight * size + size);
      }
    }
    draw_index++;
    c = buffer[draw_index];
  }
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
