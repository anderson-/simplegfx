#pragma once
#include "simpleterm.h"
#include <string.h>

int gfxt_cli_get_input(const char *question, char *buffer, int buffer_size) {
  int pager_was_enabled = gfxt_get_pager_enabled();
  gfxt_set_pager_enabled(0);
  if (question) gfxt_printf(TERM_CYAN "%s: " TERM_RESET, question);
  int index = 0;
  while (index < buffer_size - 1) {
    char c = gfxt_getchar();
    if (c == '\r' || c == '\n') {
      break;
    } else if (c == '\b' || c == 127) {
      if (index > 0) {
        index--;
        gfxt_putchar('\b');
      }
    } else {
      if (c < 32 || c > 126) continue;
      buffer[index++] = c;
      gfxt_putchar(c);
    }
  }
  buffer[index] = '\0';
  gfxt_putchar('\n');
  gfxt_set_pager_enabled(pager_was_enabled);
  return index;
}

int gfxt_cli_get_int_input(const char *question) {
  int result = 0;
  char buffer[32];
  int length = gfxt_cli_get_input(question, buffer, sizeof(buffer));
  if (length > 0) {
    result = atoi(buffer);
  }
  return result;
}

float gfxt_cli_get_float_input(const char *question) {
  float result = 0.0f;
  char buffer[32];
  int length = gfxt_cli_get_input(question, buffer, sizeof(buffer));
  if (length > 0) {
    result = atof(buffer);
  }
  return result;
}

int gfxt_cli_ask_yes_no(const char *question) {
  int pager_was_enabled = gfxt_get_pager_enabled();
  gfxt_set_pager_enabled(0);
  if (question) gfxt_printf(TERM_CYAN "%s (" TERM_GREEN "y" TERM_CYAN "/" TERM_RED "N" TERM_CYAN "): " TERM_RESET, question);
  char c = 0;
  while (c != 'y' && c != 'n' && c != 'Y' && c != 'N') {
    c = gfxt_getchar();
  }
  gfxt_printf("%c\n", c);
  gfxt_set_pager_enabled(pager_was_enabled);
  return c == 'y' || c == 'Y';
}

int gfxt_cli_select(const char *prompt, const char *question, const char **options, int option_count, int columns) {
  int pager_was_enabled = gfxt_get_pager_enabled();
  gfxt_set_pager_enabled(0);
  if (question) gfxt_printf("%s\n", question);

  if (columns <= 1) {
    for (int i = 0; i < option_count; i++) {
      gfxt_printf("  " TERM_CYAN "%d." TERM_RESET " %s\n", i + 1, options[i]);
    }
  } else {
    int rows = (option_count + columns - 1) / columns;
    int nd = (option_count >= 10) ? 2 : 1;

    int max_text[columns];
    memset(max_text, 0, sizeof(max_text));
    for (int c = 0; c < columns; c++) {
      for (int r = 0; r < rows; r++) {
        int idx = c * rows + r;
        if (idx < option_count) {
          int len = strlen(options[idx]);
          if (len > max_text[c]) max_text[c] = len;
        }
      }
    }

    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < columns; c++) {
        int idx = c * rows + r;
        if (idx < option_count) {
          int ndi = (idx + 1 >= 10) ? 2 : 1;
          gfxt_printf("  " TERM_CYAN "%d." TERM_RESET " %s", idx + 1, options[idx]);
          if (c < columns - 1) {
            int pad = (int)(nd - ndi) + (max_text[c] - (int)strlen(options[idx])) + 2;
            for (int s = 0; s < pad; s++) gfxt_putchar(' ');
          }
        }
      }
      gfxt_printf("\n");
    }
  }

  if (prompt) gfxt_printf(TERM_CYAN "%s: " TERM_RESET, prompt);
  char c = gfxt_getchar();
  if (c >= '1' && c < '1' + option_count) {
    gfxt_set_pager_enabled(pager_was_enabled);
    return c - '1';
  }
  gfxt_set_pager_enabled(pager_was_enabled);
  return -1;
}

static int cmd_cli_demo(const char *args) {
  const char *options[] = {"Option 1", "Option 2", "Option 3"};
  int selected = -1;
  while (selected == -1) {
    selected = gfxt_cli_select("Options", "Select an option", options, 3, 1);
    if (selected >= 0) {
      gfxt_printf("You selected: %s\n", options[selected]);
      break;
    } else {
      gfxt_printf("Invalid selection\n");
    }
  }
  int answer = gfxt_cli_ask_yes_no("Do you want to continue?");
  gfxt_printf("Answer: %s\n", answer ? "Yes" : "No");
  char buffer[256];
  int length = gfxt_cli_get_input("Enter your name", buffer, sizeof(buffer));
  gfxt_printf("You entered: %s (length: %d)\n", buffer, length);
  int number = gfxt_cli_get_int_input("Enter an integer number");
  gfxt_printf("You entered: %d\n", number);
  float float_number = gfxt_cli_get_float_input("Enter a float number");
  gfxt_printf("You entered: %.2f\n", float_number);
  return 0;
}

void cliutils_cmd_reg() {
  gfxt_register_cmd("clidemo", "show cli demo", cmd_cli_demo);
}
