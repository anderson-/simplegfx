#include <stdio.h>
#include "dialogutils.h"

#ifdef __cplusplus
extern "C" {
#endif

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

static int (*_dialog_draw_fn)(int width, int height);
static int return_value = 0;


void dialog_init() {
  memset(&D, 0, sizeof(D));
  gfxt_get_size(&D.term_width, &D.term_height);
  gfxt_get_drawing_params(&D.x, &D.y, &D.fsize);
  gfx_get_font_size(&D.fw, &D.fh, D.fsize);
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
  ansi_set_color(bg >= 0 ? bg : dialog_themes[D.theme][DC_BG]);
  dialog_rect(x + px, y + py, tl, 1);
  ansi_set_color(color >= 0 ? color : dialog_themes[D.theme][DC_FG]);
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
  D.csingle = D.cid;
  D.csid++;
}

void dialog_single_selection_end() {
  D.csingle = 0;
}

char * _dialog_interactive_state(int size) {
  D.cid++;
  if (!D.csingle) D.csid++;
  if (D.cstates[D.csid] == NULL) {
    D.cstates[D.csid] = (char *) malloc(size + 1);
    if (D.cstates[D.csid] == NULL) {
#ifdef DEBUG
      printf("Failed to allocate memory for dialog state\n");
#endif
      exit(1);
    }
    memset(D.cstates[D.csid], 0, size + 1);
  }
  return D.cstates[D.csid];
}

char * dialog_item_state() {
  return D.cstates[D.csid];
}

char ** dialog_raw_states(int * len) {
  if (len) *len = D.cslen;
  return D.cstates;
}

void dialog_text_button(int x, int y, int w, const char *str) {
  char * state = _dialog_interactive_state(1);
  if (D.cgrabin == D.cid) D.cgrabin = -1;
  if (D.clicked == D.cid) {
    *state = *state == '1' ? '0' : '1';
    D.clicked = -1;
  }
  dialog_set_color(D.cselected == D.cid ? DC_ACCENT : DC_BG);
  dialog_rect(x, y, w, 1);
  dialog_set_color(*state == '1' ? DC_LIGHT : DC_FG);
  dialog_text_centered(x, y, w, str);
}

static int _render_text_block(int x, int y, int inner_w, int inner_h, const char *str, int scroll_y, int *cur) {
  // \xdb pos: cur[0]=col, cur[1]=row, cur[2]=next char || ' '
  int dx = 0, dy = 0;
  if (cur) { cur[0] = -1; cur[1] = 0; cur[2] = ' '; }
  for (int i = 0; str[i]; i++) {
    if (str[i] == '\xdb') {
      if (cur) { cur[0] = dx; cur[1] = dy; cur[2] = str[i + 1] ? str[i + 1] : ' '; }
      continue;
    }
    if (str[i] == '\n') { dx = 0; dy++; continue; }
    if (dy - scroll_y >= inner_h) break;
    if (dy >= scroll_y) {
      gfx_draw_char(str[i], X(x + dx), Y(y + dy - scroll_y), D.fsize);
    }
    if (++dx >= inner_w) { dx = 0; dy++; }
  }
  return dy;
}

int dialog_text_ml(int x, int y, int w, int h, const char *str) {
  dialog_set_color(DC_FG);
  return _render_text_block(x, y, w, h, str, 0, NULL);
}

static int _count_text_lines(int inner_w, const char *str) {
  int dx = 0, dy = 1;
  for (int i = 0; str[i]; i++) {
    if (str[i] == '\xdb') continue;
    if (str[i] == '\n') { dx = 0; dy++; continue; }
    if (++dx >= inner_w) { dx = 0; dy++; }
  }
  return dy;
}

void dialog_text_scroll_area(int x, int y, int w, int h, const char *str) {
  char *state = _dialog_interactive_state(sizeof(int));
  int *scroll_y = (int *)state;
  int grabbed = D.cgrabin == D.cid;
  int selected = D.cselected == D.cid;
  if (D.clicked == D.cid) {
    D.cgrabin = D.cid;
    D.clicked = -1;
    grabbed = 1;
  }
  if (grabbed && D.input != 0) {
    int down = (D.input == EVT_KEY_DOWN || D.input == '\n' || D.input == ' ');
    int up = (D.input == EVT_KEY_UP || D.input == '\b');
    if (down && *scroll_y < _count_text_lines(w - 1, str) - h) (*scroll_y)++;
    else if (up && *scroll_y > 0) (*scroll_y)--;
    D.input = 0;
  }
  int inner_w = w - 1;
  int inner_h = h;
  int total_lines = _count_text_lines(inner_w, str);
  int max_scroll = total_lines > inner_h ? total_lines - inner_h : 0;
  if (*scroll_y > max_scroll) *scroll_y = max_scroll;
  int sb_x = x + w - 1;
  int sb_color = selected ? (grabbed ? DC_LIGHT : DC_BG) : DC_ACCENT;

  dialog_set_color(selected ? DC_ACCENT : DC_BG);
  gfx_fill_rect(X(sb_x) + D.fsize, Y(y), W(1) - D.fsize, H(h));
  dialog_set_color(*scroll_y > 0 ? sb_color : DC_BG);
  gfx_draw_char('\x18', X(sb_x) + D.fsize, Y(y), D.fsize);
  dialog_set_color(*scroll_y < max_scroll ? sb_color : DC_BG);
  gfx_draw_char('\x19', X(sb_x) + D.fsize, Y(y + h - 1), D.fsize);
  dialog_set_color(sb_color);

  if (inner_h > 2 && total_lines > inner_h) {
    int track_h = inner_h - 2;
    int bar_h = track_h * inner_h / total_lines;
    if (bar_h < 1) bar_h = 1;
    int bar_pos = max_scroll > 0 ? (track_h - bar_h) * *scroll_y / max_scroll : 0;
    gfx_fill_rect(X(sb_x) + D.fsize * 3, Y(y + 1 + bar_pos), D.fsize, H(bar_h));
  }

  dialog_set_color(DC_FG);
  _render_text_block(x, y, inner_w, inner_h, str, *scroll_y, NULL);
}

void dialog_text_area(int x, int y, int w, int h, char *label) {
  char *state = _dialog_interactive_state((w - 2) * (h - 2) + 1);
  if (D.clicked == D.cid) {
    D.cgrabin = D.cid;
    D.clicked = -1;
  }
  if (D.cgrabin != D.cid && state[0] != '\0') {
    char *s = state;
    while(*s) {
      if (*s == '\xdb') {
        while(*s) {
          *s = *(s + 1);
          s++;
        }
        break;
      }
      s++;
    }
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
      state[len] = '\xdb';
      state[len + 1] = '\0';
      cursor = len;
      len++;
    }
    if (D.input == '\n') {
      D.cgrabin = -1;
      D.cselected = D.cid;
    } else if (D.input == EVT_KEY_RIGHT) {
      if (cursor + 1 < len) {
        state[cursor] = state[cursor + 1];
        state[cursor + 1] = '\xdb';
      }
    } else if (D.input == EVT_KEY_LEFT) {
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
  int cur[3];
  _render_text_block(x + 1, y + 1, inner_w, inner_h, state, 0, cur);
  if (cur[0] >= 0 && D.cgrabin == D.cid) {
    gfx_draw_char('\xdb', X(x + 1 + cur[0]), Y(y + 1 + cur[1]), D.fsize);
    dialog_set_color(DC_LIGHT);
    gfx_draw_char((char)cur[2], X(x + 1 + cur[0]), Y(y + 1 + cur[1]), D.fsize);
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

void dialog_option(int x, int y, char *label, int type) {
  char * state = _dialog_interactive_state(1);
  int option_id = D.csingle ? D.cid - D.csingle - 1 : 0;
  if (D.cgrabin == D.cid) D.cgrabin = -1;
  if (D.clicked == D.cid) {
    if (type == DIALOG_OPT_CHECKBOX) {
      *state = *state == '1' ? '0' : '1';
    } else {
      *state = '0' + option_id;
    }
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
  int toggled = D.csingle ? *state == '0' + option_id : *state == '1';
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

void _dialog_draw() {
  if (_dialog_draw_fn) {
    return_value = _dialog_draw_fn(D.term_width, D.term_height);
  }
}

void dialog_close() {
  gfxt_set_overlay(NULL);
  _dialog_draw_fn = NULL;
  gfxt_on_key('\r');
}

void dialog_free_data() {
  for (int i = 0; i < D.cslen; i++) {
    if (D.cstates[i]) {
      free(D.cstates[i]);
      D.cstates[i] = NULL;
    }
  }
}

int dialog_show(int (*dialog_fn)(int width, int height)) {
  D.cselected = -1;
  D.clicked = -1;
  D.cgrabin = -1;
  D.csingle = 0;
  return_value = 0;
  boxdrawing = 1;
  dialog_free_data();
  _dialog_draw_fn = dialog_fn;
  gfxt_set_overlay(_dialog_draw);
  while (1) {
    gfxt_refresh_display();
    char c = gfxt_getchar();
    if (!_dialog_draw_fn) break;
#ifdef DEBUG
    printf("received %d - %c\n", c, c);
#endif
    if (D.cgrabin != -1) {
      if (c == EVT_KEY_ESCAPE || c == '`') {
        D.cgrabin = -1;
        continue;
      } else if (c == '\t') {
        D.cgrabin = -1;
        D.cselected = (D.cselected + 1) % D.cslen;
        continue;
      }
      D.input = c;
    } else {
      switch (c) {
        case EVT_KEY_ESCAPE:
        case '`':
          dialog_close();
          return return_value;
#ifdef DEBUG
        case 'w':
          {
            for (int i = 0; i < D.cslen; i++) {
              printf("State %d: %s\n", i, D.cstates[i]);
            }
          }
          break;
#endif
        case '\r':
        case '\n':
        case ' ':
          D.clicked = D.cselected;
#ifdef DEBUG
          printf("Clicked: %d\n", D.clicked);
#endif
          break;
        case EVT_KEY_DOWN:
        case EVT_KEY_RIGHT:
        case '.':
        case '/':
        case '\t':
          D.cselected++;
          break;
        case EVT_KEY_UP:
        case EVT_KEY_LEFT:
        case ';':
        case ',':
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
#ifdef DEBUG
      printf("Selected: %d\n", D.cselected);
#endif
    }
  }
  dialog_close();
  return return_value;
}

#ifdef __cplusplus
}
#endif
