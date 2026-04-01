#pragma once

#include <stdio.h>
#include "simplegfx.h"
#include "ansiutils.h"
#include "simpleterm.h"
#include "simplefont.h"
#include <string.h>

#define DC_BG 0
#define DC_FG 1
#define DC_ACCENT 2
#define DC_LIGHT 3

// { bg, fg, accent, light }
static const uint16_t dialog_themes[][4] = {
  { 7,  0,  4, 15},
  { 7,  0,  1, 15},
  { 7,  0,  4, 14},
  { 6,  0, 9, 15},
  { 4,  0, 1, 14},
  { 1,  0, 14, 15},
  { 8,  0, 10, 15},
};

#define NUM_DIALOG_THEMES (sizeof(dialog_themes) / sizeof(dialog_themes[0]))

struct {
  int x;
  int y;
  int fsize;
  int theme;
  int fw;
  int fh;
  int term_width;
  int term_height;
  int cid;
  int csid;
  int clen;
  int cselected;
  int clicked;
  int cgrabin;
  int csingle;
  int input;
  char * cstates[24];
  int cslen;
} D;

static int visible = 0;
static void (*dialog)(void) = NULL;

static int dialog_draw() {
  if (!dialog) return 0;
  dialog();
  return visible;
}

void dialog_init() {
  memset(&D, 0, sizeof(D));
  gfxt_get_size(&D.term_width, &D.term_height);
  gfxt_get_drawing_params(&D.x, &D.y, &D.fsize);
  gfx_get_font_size(&D.fw, &D.fh, D.fsize);
  gfxt_set_overlay(dialog_draw);
}

void dialog_set_theme(int theme) {
  if (theme < 0) {
    dialog_set_theme((D.theme + 1) % NUM_DIALOG_THEMES);
    return;
  }
  if (theme >= (int) NUM_DIALOG_THEMES) return;
  D.theme = theme;
}


void dialog_set_color(int color) {
  ansi_set_color(dialog_themes[D.theme][color]);
}

#define X(__x) ((__x) * D.fw + D.x)
#define W(__w) ((__w) * D.fw)
#define Y(__y) ((__y) * D.fh + D.y)
#define H(__h) ((__h) * D.fh)

void dialog_rect(int x, int y, int w, int h) {
  gfx_fill_rect(X(x), Y(y) - D.fsize, W(w), H(h));
}

int dialog_text(int x, int y, const char *str) {
  gfx_text(str, X(x), Y(y), D.fsize);
  return strlen(str);
}

int dialog_printf(int x, int y, const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  dialog_text(x, y, buf);
  return len;
}

void dialog_panel_frame_simple_composite(int x, int y, int w, int h, int lc, int rc) {
  dialog_set_color(lc);
  dialog_text(x, y, BOX_TL);
  for (int l = 0; l < h - 2; l++) {
    dialog_text(x, y + l + 1, BOX_V);
  }
  dialog_text(x, y + h - 1, BOX_BL);
  for (int c = 0; c < w - 2; c++) {
    dialog_text(x + c + 1, y, BOX_H);
  }

  dialog_set_color(rc);
  dialog_text(x + w - 1, y, BOX_TR);
  for (int l = 0; l < h - 2; l++) {
    dialog_text(x + w - 1, y + l + 1, BOX_V);
  }
  dialog_text(x + w - 1, y + h - 1, BOX_BR);
  for (int c = 0; c < w - 2; c++) {
    dialog_text(x + c + 1, y + h - 1, BOX_H);
  }
}

void dialog_panel_frame_simple(int x, int y, int w, int h, int level) {
  if (level > 0) {
    dialog_panel_frame_simple_composite(x, y, w, h, DC_LIGHT, DC_FG);
  } else if (level < 0) {
    dialog_panel_frame_simple_composite(x, y, w, h, DC_FG, DC_LIGHT);
  } else {
    dialog_panel_frame_simple_composite(x, y, w, h, DC_FG, DC_FG);
  }
}

void dialog_panel_split(int x, int y, int w) {
  dialog_set_color(DC_LIGHT);
  dialog_text(x, y, BOX_LM);
  for (int c = 1; c < w - 1; c++) {
    dialog_text(x + c, y, BOX_H);
  }
  dialog_set_color(DC_FG);
  dialog_text(x + w - 1, y, BOX_RM);
}

#define PANEL_ITEXT_TITLE 1
#define PANEL_ITEXT_TL 2
#define PANEL_ITEXT_TR 3
#define PANEL_ITEXT_BL 4
#define PANEL_ITEXT_BR 5
#define PANEL_ITEXT_FOOTER 6

void dialog_panel_inline_text(int type, int color, int bg, int x, int y, int w, int h, char *text) {
  int px = 0;
  int py = 0;
  int tl = strlen(text);
  switch (type) {
    case PANEL_ITEXT_TITLE:
      px = (w - tl) / 2;
      py = 0;
      break;
    case PANEL_ITEXT_FOOTER:
      px = (w - tl) / 2;
      py = h - 1;
      break;
    case PANEL_ITEXT_TL:
      px = 2;
      py = 0;
      break;
    case PANEL_ITEXT_TR:
      px = w - tl - 2;
      py = 0;
      break;
    case PANEL_ITEXT_BL:
      px = 2;
      py = h - 1;
      break;
    case PANEL_ITEXT_BR:
      px = w - tl - 2;
      py = h - 1;
      break;
  }
  if (bg >= 0) {
    ansi_set_color(bg);
    dialog_rect(x + px, y + py, tl, 1);
  }
  if (color >= 0) {
    ansi_set_color(color);
  }
  dialog_text(x + px, y + py, text);
}

void dialog_text_centered(int x, int y, int w, const char *str) {
  int tl = strlen(str);
  int px = (w - tl) / 2;
  dialog_text(x + px, y, str);
}


void dialog_form_start() {
  D.cid = -1;
  D.csid = -1;
  D.clen = 0;
}

void dialog_form_end() {
  D.clen = D.cid + 1;
  D.cslen = D.csid + 1;
}

void dialog_single_selection_start() {
  D.csingle = 1;
  D.csid++;
}

void dialog_single_selection_end() {
  D.csingle = 0;
}

static char * _dialog_interactive_state(int size) {
  D.cid++;
  if (!D.csingle) D.csid++;
  if (D.cstates[D.csid] == NULL) {
    D.cstates[D.csid] = (char *) malloc(size + 1);
    if (D.cstates[D.csid] == NULL) {
      printf("Failed to allocate memory for dialog state\n");
      exit(1);
    }
    memset(D.cstates[D.csid], 0, size + 1);
  }
  return D.cstates[D.csid];
}

void dialog_text_button(int x, int y, int w, const char *str) {
  char * state = _dialog_interactive_state(1);
  if (D.cgrabin == D.cid) D.cgrabin = -1;
  if (D.clicked == D.cid) {
    *state = *state == '1' ? '0' : '1';
    printf("State: %s (%s)\n", state, str);
    D.clicked = -1;
  }
  dialog_set_color(D.cselected == D.cid ? DC_ACCENT : DC_BG);
  dialog_rect(x, y, w, 1);
  dialog_set_color(*state == '1' ? DC_LIGHT : DC_FG);
  dialog_text_centered(x, y, w, str);
}

void dialog_text_area(int x, int y, int w, int h, char *label) {
  char *state = _dialog_interactive_state((w - 2) * (h - 2) + 1);
  if (D.clicked == D.cid) {
    D.cgrabin = D.cid;
    D.clicked = -1;
  }
  dialog_panel_frame_simple(x, y, w, h, -1);
  int snotgrab = D.cgrabin != D.cid && D.cselected == D.cid;
  int color = dialog_themes[D.theme][snotgrab? DC_LIGHT : DC_FG];
  int bg = dialog_themes[D.theme][snotgrab ? DC_ACCENT : DC_BG];
  dialog_panel_inline_text(PANEL_ITEXT_TL, color, bg, x, y, w, h, label);
  if (D.cgrabin == D.cid && D.input != 0) {
    int cursor = -1;
    int len = strlen(state);
    for (int i = 0; i < len; i++) {
      if (state[i] == '\xdb') { cursor = i; break; }
    }
    if (cursor == -1) {
      state[0] = '\xdb';
      state[1] = '\0';
      cursor = 0;
      len = 1;
    }
    if (D.input == '\n') {
      D.cgrabin = -1;
      D.cselected = D.cid;
    } else if (D.input == ANSI_CURSOR_RIGHT) {
      if (cursor + 1 < len) {
        state[cursor] = state[cursor + 1];
        state[cursor + 1] = '\xdb';
      }
    } else if (D.input == ANSI_CURSOR_LEFT) {
      if (cursor > 0) {
        state[cursor] = state[cursor - 1];
        state[cursor - 1] = '\xdb';
      }
    } else if (D.input == '\b') {
      if (cursor > 0) {
        for (int j = cursor - 1; j < len - 1; j++)
          state[j] = state[j + 1];
        state[len - 1] = '\0';
      }
    } else if (len < (w - 2) * (h - 2)) {
      for (int j = len; j > cursor; j--)
        state[j] = state[j - 1];
      state[cursor] = D.input;
      state[len + 1] = '\0';
    }

    D.input = 0;
  }
  dialog_set_color(DC_FG);
  int inner_w = w - 2;
  int inner_h = h - 2;
  int dx = 0, dy = 0;
  int cur_dx = 0, cur_dy = 0;
  char cur_char = ' ';
  for (int i = 0; i < (int)strlen(state); i++) {
    if (dy >= inner_h) break;
    if (state[i] == '\xdb') {
      cur_dx = dx;
      cur_dy = dy;
      cur_char = state[i + 1] ? state[i + 1] : ' ';
      continue;
    }
    gfx_draw_char(state[i], X(x + 1 + dx), Y(y + 1 + dy), D.fsize);
    dx++;
    if (dx >= inner_w) { dx = 0; dy++; }
  }
  if (cur_dx >= 0 && D.cgrabin == D.cid) {
    gfx_draw_char('\xdb', X(x + 1 + cur_dx), Y(y + 1 + cur_dy), D.fsize);
    dialog_set_color(DC_LIGHT);
    gfx_draw_char(cur_char, X(x + 1 + cur_dx), Y(y + 1 + cur_dy), D.fsize);
  }
}

void dialog_input(int x, int y, int w, char *label) {
  dialog_text_area(x, y, w, 3, label);
}

void dialog_toggle_button(int x, int y, int w, const char *str) {
  char * state = _dialog_interactive_state(1);
  if (D.cgrabin == D.cid) D.cgrabin = -1;
  if (D.clicked == D.cid) {
    *state = *state == '1' ? '0' : '1';
    printf("State: %s (%s)\n", state, str);
    D.clicked = -1;
  }

  dialog_panel_frame_simple(x, y, w, 3, *state == '1' ? -1 : 1);
  if (D.cselected == D.cid) {
    dialog_set_color(DC_ACCENT);
    dialog_rect(x + 1, y + 1, w - 2, 1);
  }
  dialog_set_color(*state == '1' ? DC_LIGHT : DC_FG);
  dialog_text_centered(x + 1, y + 1, w - 2, str);
}

#define DIALOG_OPT_NONE     0
#define DIALOG_OPT_SC       1
#define DIALOG_OPT_CHECKBOX 2
#define DIALOG_OPT_RADIO    3

void dialog_option(int x, int y, char *label, int type) {
  char * state = _dialog_interactive_state(1);
  int option_id = D.cid - D.csid;
  if (D.cgrabin == D.cid) D.cgrabin = -1;
  if (D.clicked == D.cid) {
    *state = '0' + option_id;
    D.clicked = -1;
  }
  if (D.cselected == D.cid) {
    dialog_set_color(DC_ACCENT);
    if (type == DIALOG_OPT_SC) {
      dialog_rect(x + 2, y, strlen(label) + 2, 1);
    } else {
      dialog_rect(x, y, strlen(label) + 4, 1);
    }
  }
  int toggled = *state == '0' + option_id;
  dialog_set_color(D.cselected == D.cid ? DC_LIGHT : DC_FG);
  switch (type) {
    case DIALOG_OPT_CHECKBOX:
      dialog_printf(x, y, "[%c] %s", toggled ? 'x' : ' ', label);
      break;
    case DIALOG_OPT_RADIO:
      dialog_printf(x, y, "(%c) %s", toggled ? 'o' : ' ', label);
      break;
    case DIALOG_OPT_SC:
      if (toggled) {
        dialog_printf(x + 1, y, " >  %s", label + 1);
      } else {
        dialog_printf(x + 1, y, "    %s", label + 1);
      }
      dialog_set_color(D.cselected == D.cid ? DC_LIGHT : DC_ACCENT);
      gfx_draw_char(label[0], X(x + 4), Y(y), D.fsize);
      break;
    default:
      dialog_printf(x, y, "%s", label);
      break;
  }
}

void dialog_show(void (*dialog_fn)()) {
  visible = 1;
  D.cselected = -1;
  D.clicked = -1;
  D.cgrabin = -1;
  D.csingle = 0;
  for (int i = 0; i < D.cslen; i++) {
    if (D.cstates[i]) {
      free(D.cstates[i]);
    }
    D.cstates[i] = NULL;
  }
  dialog = dialog_fn;
  while (1) {
    char c = gfxt_getchar();
    printf("received %d - %c\n", c, c);
    if (D.cgrabin != -1) {
      if (c == 27) {
        D.cgrabin = -1;
        D.cselected = D.cselected;
        continue;
      }
      D.input = c;
      // add esc to exit
    } else {
      switch (c) {
        case 'q':
          visible = 0;
          return;
        case 'w':
          {
            for (int i = 0; i < D.cslen; i++) {
              printf("State %d: %s\n", i, D.cstates[i]);
            }
          }
          break;
        case '\r':
        case '\n':
          D.clicked = D.cselected;
          printf("Clicked: %d\n", D.clicked);
          break;
        case ANSI_CURSOR_DOWN:
        case ANSI_CURSOR_RIGHT:
          D.cselected++;
          break;
        case ANSI_CURSOR_UP:
        case ANSI_CURSOR_LEFT:
          D.cselected--;
          break;
        default:
          D.cgrabin = D.cselected;
          D.input = c;
          break;
      }
      if (D.cselected >= D.clen) {
        D.cselected = 0;
      } else if (D.cselected < 0) {
        D.cselected = D.clen - 1;
      }
      printf("Selected: %d\n", D.cselected);
    }
  }
  visible = 0;
}


void dialog_overlay_demo() {
  boxdrawing = 1;

  int width = D.term_width - 4;
  int height = D.term_height - 2;

  int x = 2;
  int y = 1;

  dialog_set_color(DC_BG);
  dialog_rect(x, y, width, height);
  dialog_panel_frame_simple(x, y, width, height, 1);

  dialog_panel_inline_text(PANEL_ITEXT_TITLE, 9, 7, x, y, width, height, "title");
  dialog_panel_inline_text(PANEL_ITEXT_TL, 10, 7, x, y, width, height, "tl");
  dialog_panel_inline_text(PANEL_ITEXT_TR, 11, 7, x, y, width, height, "tr");
  dialog_panel_inline_text(PANEL_ITEXT_BL, 12, 7, x, y, width, height, "bl");
  dialog_panel_inline_text(PANEL_ITEXT_BR, 13, 7, x, y, width, height, "br");
  dialog_panel_inline_text(PANEL_ITEXT_FOOTER, 14, 7, x, y, width, height, "footer");

  x += 2;
  y += 1;

  dialog_panel_frame_simple(x, y, width - 4, 5, -1);

  dialog_form_start();
  y += 1;
  dialog_toggle_button(5, y, 10, "Button 1");
  dialog_toggle_button(16, y, 10, "Button 2");
  y += 4;

  x = 2;
  x += 2;
  dialog_input(x, y, width / 2 - 2, "input");
  x += width / 2 - 1;
  dialog_text_area(x, y, width / 2 - 2, 4, "text area");

  y += 3;
  x = 2;
  x += 1;
  dialog_single_selection_start();
  dialog_option(x, y + 0, "A", DIALOG_OPT_SC);
  dialog_option(x, y + 1, "B", DIALOG_OPT_SC);
  dialog_option(x, y + 2, "C", DIALOG_OPT_SC);
  dialog_single_selection_end();

  x += 7;
  dialog_option(x, y + 0, "X", DIALOG_OPT_CHECKBOX);
  dialog_single_selection_start();
  dialog_option(x, y + 1, "Y", DIALOG_OPT_RADIO);
  dialog_option(x, y + 2, "Z", DIALOG_OPT_RADIO);
  dialog_single_selection_end();

  y = height - 1;
  x = 2;
  dialog_panel_split(x, y, width);

  x += 3 * width / 4 - 10;
  dialog_text_button(x, y, 8, "<CANCEL>");
  x += 10;
  dialog_text_button(x, y, 4, "<OK>");

  dialog_form_end();

  boxdrawing = 0;
}

static int cmd_dialog_demo(const char *args) {
  dialog_show(dialog_overlay_demo);
  return 0;
}

static int cmd_dialog_theme(const char *args) {
  int id = -1;
  sscanf(args, "%d", &id);
  dialog_set_theme(id);
  if (id >= 0) {
    gfxt_printf(TERM_GREEN "dialog theme set to %d" TERM_RESET "\n", id);
  } else {
    gfxt_printf(TERM_GREEN "dialog theme switched" TERM_RESET "\n");
  }
  return 0;
}

void dialog_cmd_reg() {
  gfxt_register_cmd("dlgdemo", "show dialog demo", cmd_dialog_demo);
  gfxt_register_cmd("dlgtheme", "set dialog theme", cmd_dialog_theme);
}