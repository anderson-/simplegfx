#pragma once

#include <stdio.h>
#include "ansiutils.h"
#include "dialogutils.h"
#include <string.h>

int dialog_overlay_demo(int width, int height) {
  width -= 4;
  height -= 2;

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
  if (dialog_item_state()[0] == '1') {
    dialog_close();
    return 0;
  }
  x += 10;
  dialog_text_button(x, y, 4, "<OK>");
  if (dialog_item_state()[0] == '1') {
    dialog_close();
    return 1;
  }

  dialog_form_end();
  return 0;
}

static int cmd_dialog_demo(const char *args) {
  if (dialog_show(dialog_overlay_demo) == 1) {
    gfxt_printf(TERM_GREEN "OK pressed" TERM_RESET "\n");
  } else {
    gfxt_printf(TERM_RED "CANCEL pressed" TERM_RESET "\n");
  }
  int len;
  char **states = dialog_raw_states(&len);
  for (int i = 0; i < len; i++) {
    gfxt_printf("state[%d]: %s\n", i, *states[i] ? states[i] : "(null)");
  }
  dialog_free_data();
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

int dialog_overlay_scroll(int width, int height) {
  width -= 4;
  height -= 2;

  int x = 2;
  int y = 1;

  dialog_set_color(DC_BG);
  dialog_rect(x, y, width, height);
  dialog_panel_frame_simple(x, y, width, height, 1);
  dialog_panel_inline_text(PANEL_ITEXT_TITLE, -1, -1, x, y, width, height, "scroll area");

  dialog_form_start();

  dialog_text_scroll_area(x + 1, y + 1, width - 1, height - 2,
    "Line 1: lorem ipsum dolor sit amet.\n"
    "Line 2: consectetur adipiscing elit.\n"
    "Line 3: sed do eiusmod tempor incididunt ut labore.\n"
    "Line 4: et dolore magna aliqua.\n"
    "Line 5: ut enim ad minim veniam.\n"
    "Line 6: quis nostrud exercitation ullamco laboris.\n"
    "Line 7: nisi ut aliquip ex ea commodo consequat.\n"
    "Line 8: duis aute irure dolor in reprehenderit.\n"
    "Line 9: in voluptate velit esse cillum dolore eu fugiat nulla pariatur.\n"
    "Line 10: excepteur sint occaecat cupidatat non proident.\n"
    "Line 11: sunt in culpa qui officia deserunt mollit anim id est laborum.");

  dialog_form_end();
  return 0;
}

static int cmd_dialog_scroll(const char *args) {
  dialog_show(dialog_overlay_scroll);
  return 0;
}

void dialog_cmd_reg() {
  gfxt_register_cmd("dlgdemo",   "show dialog demo",      cmd_dialog_demo);
  gfxt_register_cmd("dlgtheme",  "set dialog theme",      cmd_dialog_theme);
  gfxt_register_cmd("dlgscroll", "show scroll area demo", cmd_dialog_scroll);
}