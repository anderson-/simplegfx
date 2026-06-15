#pragma once

#include "ansiutils.h"
#include "simpleterm.h"
#include "dialogutils.h"

uint16_t simplehash(const char *str) {
  uint16_t hash = 0;
  while (*str) {
    hash = hash * 31 + *str++;
  }
  return hash;
}

int cmd_login(const char *args) {
  static char username[32] = {0};
  static uint16_t password_hash = 0;

  int ask_username = 1;
  if (args[0] != '\0') {
    gfxt_printf("[%s]", args);
    if (strcmp(args, username) != 0) {
      return 1;
    }
    ask_username = 0;
  }
  if (ask_username) {
    gfxt_printf(TERM_CYAN "username: " TERM_RESET);
    cmd_read("0");
    if (strlen(username) == 0) {
      strcpy(username, input_buffer);
      gfxt_printf("name set to: %s\n", username);
    } else if (strcmp(username, input_buffer) != 0) {
      return 1;
    }
  }
  gfxt_printf(TERM_CYAN "password: " TERM_RESET);
  cmd_read("1");
  if (password_hash == 0) {
    password_hash = simplehash(input_buffer);
  } else if (simplehash(input_buffer) != password_hash) {
    return 1;
  }
  gfxt_printf(TERM_GREEN "login successful\n" TERM_RESET);
  return 0;
}

int cmd_ansi(const char *args) {
  int row, col, n;
  for (row = 0; row < 11; row++) {
    for (col = 0; col < 10; col++) {
      n = 10 * row + col;
      if (n > 109) break;
      gfxt_printf("\033[%dm %3d\033[m", n, n);
    }
    gfxt_println("");
  }
  return 0;
}

int cmd_palette(const char *args) {
  for (int bg = 40; bg <= 47; bg++) {
    gfxt_printf("%d \x1b[%dm", bg, bg);
    for (int fg = 30; fg <= 37; fg++) {
      gfxt_printf("\x1b[%dm%d ", fg, fg);
    }
    gfxt_printf("\x1b[m\n");
  }
  gfxt_printf("40 \x1b[m");
  for (int i = 90; i <= 97; i++) {
    gfxt_printf("\x1b[%dm%d ", i, i);
  }
  gfxt_printf("\x1b[m\n");
  return 0;
}

int cmd_ascii(const char *args) {
  int w;
  gfxt_get_size(&w, NULL);
  int max_cols = (w - 2) / 5;
  for (int i = 0; i < 256; i++) {
    if ((i == 0 || i == '\x1b' || i == '\n' || i == '\r' || i == '\b' || i == '\t' || i == '\x7f')) {
      gfxt_printf(TERM_BBLACK "%02x " TERM_YELLOW "! ", i);
    } else {
      gfxt_printf(TERM_BBLACK "%02x " TERM_RESET "%c ", i, i);
    }
    if ((i + 1) % max_cols == 0) gfxt_println("");
  }
  gfxt_println("");
  return 0;
}

int cmd_boxdrawing(const char *args) {
  boxdrawing = !boxdrawing;
  gfxt_println("\xda\xc4\xc2\xbf  \xc9\xcd\xcb\xbb  \xd6\xc4\xd2\xb7  \xd5\xcd\xd1\xb8");
  gfxt_println("\xb3 \xb3\xb3  \xba \xba\xba  \xba \xba\xba  \xb3 \xb3\xb3");
  gfxt_println("\xc3\xc4\xc5\xb4  \xcc\xcd\xce\xb9  \xc7\xc4\xd7\xb6  \xc6\xcd\xd8\xb5");
  gfxt_println("\xc0\xc4\xc1\xd9  \xc8\xcd\xca\xbc  \xd3\xc4\xd0\xbd  \xd4\xcd\xcf\xbe");
  gfxt_println("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf");
  gfxt_println("\xb3  \xc9\xcd\xcd\xcd\xbb Some Text  \xb3\xb1");
  gfxt_println("\xb3  \xc8\xcd\xcb\xcd\xbc in the box \xb3\xb1");
  gfxt_println("\xc6\xcd\xd1\xcd\xcd\xca\xcd\xcd\xd1\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xb5\xb1");
  gfxt_println("\xb3 \xc3\xc4\xc4\xc2\xc4\xc4\xb4           \xb3\xb1");
  gfxt_println("\xb3 \xc0\xc4\xc4\xc1\xc4\xc4\xd9           \xb3\xb1");
  gfxt_println("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9\xb1");
  gfxt_println(" \xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1");
  return 0;
}

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

void gfxt_test_cmd_reg() {
  gfxt_register_cmd("login", "login", cmd_login);
  gfxt_register_cmd("ansi", "ANSI color codes", cmd_ansi);
  gfxt_register_cmd("palette", "print color palette", cmd_palette);
  gfxt_register_cmd("ascii", "print ASCII table", cmd_ascii);
  gfxt_register_cmd("boxdrawing", "print box drawing characters", cmd_boxdrawing);
  gfxt_register_cmd("dlgdemo", "show dialog demo", cmd_dialog_demo);
  gfxt_register_cmd("dlgtheme", "set dialog theme", cmd_dialog_theme);
  gfxt_register_cmd("dlgscroll", "show scroll area demo", cmd_dialog_scroll);
}
