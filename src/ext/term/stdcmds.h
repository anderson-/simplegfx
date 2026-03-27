#pragma once

#include "ansiutils.h"
#include "simpleterm.h"
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

char input_buffer[128] = {0};
char username[32] = {0};
uint16_t password_hash = 0;

uint16_t simplehash(const char *str) {
  uint16_t hash = 0;
  while (*str) {
    hash = hash * 31 + *str++;
  }
  return hash;
}

int cmd_clear(const char *args) {
  gfxt_clear();
  return 0;
}

int cmd_echo(const char *args) {
  if (!args) {
    gfxt_print("");
    return 0;
  }

  char buffer[128];
  char *dst = buffer;
  const char *src = args;

  while (*src && dst < buffer + sizeof(buffer) - 1) {
    if (*src == '\\' && *(src+1) == 'x') {
      int val;
      if (sscanf(src + 2, "%2x", &val) == 1) {
        *dst++ = (char)val;
        src += 4;
      } else {
        *dst++ = *src++;
      }
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
  gfxt_print(buffer);
  return 0;
}

int cmd_error(const char *args) {
  if (*args) gfxt_printf(TERM_RED "%s" TERM_RESET "\n", args);
  return 1;
}

int cmd_read(const char *args) {
  int hidden = 0;
  sscanf(args, "%d", &hidden);
  int i = 0;
  while(1) {
    char c = gfxt_getchar();
    if (c == '\n' || c == '\r') break;
    input_buffer[i++] = c;
    if (hidden) {
      gfxt_putchar('*');
    } else {
      gfxt_putchar(c);
    }
  }
  input_buffer[i] = '\0';
  gfxt_putchar('\n');
  return i == 0;
}

int cmd_eval(const char *args) {
  char cmd[32] = {0};
  char args_buf[128] = {0};
  if (sscanf(args, "%31s %127[^\n]", cmd, args_buf) < 1) {
    return 1;
  }
  for (int i = 0; i < gfxt_cmd_registry_len; i++) {
    if (strcmp(gfxt_cmd_registry[i].name, cmd) == 0) {
      return gfxt_cmd_registry[i].func(args_buf);
    }
  }
  return 1;
}

int cmd_time(const char *args) {
  struct timeval start;
  gettimeofday(&start, NULL);
  int code = cmd_eval(args);
  struct timeval end;
  gettimeofday(&end, NULL);
  gfxt_printf("time: %.3f seconds\n", (float)(end.tv_sec - start.tv_sec) + (float)(end.tv_usec - start.tv_usec) / 1000000.0);
  return code;
}

int cmd_login(const char *args) {
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
  int max = 256;
  sscanf(args, "%d", &max);
  if (max < 0) max = 0;
  if (max > 256) max = 256;
  for (int i = 0; i < max; i++) {
    gfxt_printf(TERM_BBLACK "%02x " TERM_RESET "%c ", i, i);
    if ((i + 1) % 4 == 0) gfxt_println("");
  }
  return 0;
}

int cmd_theme(const char *args) {
  int id = -1;
  sscanf(args, "%d", &id);
  gfxt_set_theme(id);
  if (id >= 0) {
    gfxt_printf(TERM_GREEN "theme set to %d" TERM_RESET "\n", id);
  } else {
    gfxt_printf(TERM_GREEN "theme switched" TERM_RESET "\n");
  }
  return 0;
}

int cmd_date(const char *args) {
  time_t now = time(NULL);
  if (now > 1000000000) {
    char dt[32];
    struct tm ti;
    localtime_r(&now, &ti);
    strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &ti);
    gfxt_println(dt);
  } else {
    gfxt_println(TERM_RED "time not synced" TERM_RESET);
    return 1;
  }
  return 0;
}

int cmd_boxdrawing(const char *args) {
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

int cmd_help(const char *args) {
  for (int i = 0; i < gfxt_cmd_registry_len; i++) {
    gfxt_printf(TERM_CYAN " %s" TERM_RESET " \x1a %s\n", gfxt_cmd_registry[i].name, gfxt_cmd_registry[i].help);
  }
  return 0;
}

void gfxt_std_cmd_reg() {
  gfxt_register_cmd("clear", "clear screen", cmd_clear);
  gfxt_register_cmd("echo", "print text", cmd_echo);
  gfxt_register_cmd("error", "print error message", cmd_error);
  gfxt_register_cmd("read", "read input", cmd_read);
  gfxt_register_cmd("eval", "evaluate command", cmd_eval);
  gfxt_register_cmd("time", "time command execution", cmd_time);
  gfxt_register_cmd("login", "login", cmd_login);
  gfxt_register_cmd("ansi", "ANSI color codes", cmd_ansi);
  gfxt_register_cmd("palette", "print color palette", cmd_palette);
  gfxt_register_cmd("ascii", "print ASCII table", cmd_ascii);
  gfxt_register_cmd("theme", "[id] set color theme", cmd_theme);
  gfxt_register_cmd("date", "current date/time", cmd_date);
  gfxt_register_cmd("boxdrawing", "print box drawing characters", cmd_boxdrawing);
  gfxt_register_cmd("help", "list available commands", cmd_help);
}