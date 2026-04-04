#pragma once
#include <stdio.h>
#include "simplegfx.h"
#include "ansiutils.h"
#include "simpleterm.h"
#include "simplefont.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

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

void dialog_init();
void dialog_set_theme(int theme);
void dialog_set_color(int color);

void dialog_rect(int x, int y, int w, int h);
int dialog_text(int x, int y, const char *str);
int dialog_printf(int x, int y, const char *fmt, ...);

void dialog_panel_frame_simple_composite(int x, int y, int w, int h, int lc, int rc);
void dialog_panel_frame_simple(int x, int y, int w, int h, int level);
void dialog_panel_split(int x, int y, int w);

#define PANEL_ITEXT_TITLE 1
#define PANEL_ITEXT_TL 2
#define PANEL_ITEXT_TR 3
#define PANEL_ITEXT_BL 4
#define PANEL_ITEXT_BR 5
#define PANEL_ITEXT_FOOTER 6

void dialog_panel_inline_text(int type, int color, int bg, int x, int y, int w, int h, char *text);
void dialog_text_centered(int x, int y, int w, const char *str);
void dialog_form_start();
void dialog_form_end();
void dialog_single_selection_start();
void dialog_single_selection_end();

char * _dialog_interactive_state(int size);

char * dialog_item_state();
char ** dialog_raw_states(int * len);

void dialog_text_button(int x, int y, int w, const char *str);
int dialog_text_ml(int x, int y, int w, int h, const char *str);
void dialog_text_scroll_area(int x, int y, int w, int h, const char *str);
void dialog_text_area(int x, int y, int w, int h, char *label);
void dialog_input(int x, int y, int w, char *label);
void dialog_toggle_button(int x, int y, int w, const char *str);

#define DIALOG_OPT_NONE     0
#define DIALOG_OPT_SC       1
#define DIALOG_OPT_CHECKBOX 2
#define DIALOG_OPT_RADIO    3

void dialog_option(int x, int y, char *label, int type);

void dialog_close();
void dialog_free_data();
int dialog_show(int (*dialog_fn)(int width, int height));

#ifdef __cplusplus
}
#endif
