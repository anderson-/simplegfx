#pragma once

#include "ansiutils.h"
#include "simpleterm.h"
#include <time.h>
#include <stdarg.h>

char input_buffer[128] = {0};

int cmd_sleep(const char *args) {
  float s = 0.0;
  if (sscanf(args, "%f", &s) == 1 && s > 0.0) {
    gfx_wait((int)(s * 1000));
  } else {
    gfxt_println(TERM_RED "usage: sleep <seconds>" TERM_RESET);
  }
  return 0;
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
  uint32_t start = gfx_time();
  int code = cmd_eval(args);
  gfxt_printf("time: %.3f seconds\n", gfx_dt(start) / 1000.0);
  return code;
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

int cmd_beep(const char *args) {
  int frequency = 440;
  int duration = 50;
  sscanf(args, "%d %d", &frequency, &duration);
  gfx_beep(frequency, duration);
  return 0;
}

int cmd_watch(const char *args) {
  float sec = 1.0;
  int n = 0;
  char cmd[128] = {0};

  int matched = sscanf(args, "%f %d %127[^\n]", &sec, &n, cmd);
  if (sec < 0.05) sec = 0.05;
  int has_cmd = (matched >= 3 && cmd[0] != '\0');

  gfxt_printf("watch every %.2fs", sec);
  if (n > 0) gfxt_printf(" x %d", n);
  if (has_cmd) gfxt_printf(": %s", cmd);
  gfxt_printf("\n[press any key to stop]\n");

  int count = 0;
  while (n == 0 || count < n) {
    if (has_cmd) {
      gfxt_run_cmd(cmd);
    } else {
      gfx_beep(880, 50);
    }
    count++;
    gfxt_stdin = 0;
    int target = (int)(sec * 1000);
    int waited = 0;
    while (waited < target) {
      int chunk = 100;
      if (target - waited < chunk) chunk = target - waited;
      gfx_wait(chunk);
      waited += chunk;
      if (gfxt_stdin) { gfxt_stdin = 0; gfxt_println("cancelled"); return 0; }
    }
  }
  return 0;
}

int cmd_help(const char *args) {
  for (int i = 0; i < gfxt_cmd_registry_len; i++) {
    gfxt_printf(TERM_CYAN " %s" TERM_RESET " \x1a %s\n", gfxt_cmd_registry[i].name, gfxt_cmd_registry[i].help);
  }
  return 0;
}

static void bar_draw(int w, int n, ...) {
  va_list args;
  va_start(args, n);
  const char *name[n], *color[n];
  int value[n], total = 0;
  for (int i = 0; i < n; i++) {
    name[i]  = va_arg(args, const char *);
    color[i] = va_arg(args, const char *);
    total   += value[i] = va_arg(args, int);
  }
  va_end(args);
  if (total < 1) total = 1;
  int rest = w;
  for (int i = 0; i < n; i++) {
    int seg = value[i] * w / total;
    if (seg > rest) seg = rest;
    gfxt_print(color[i]);
    for (int j = 0; j < seg; j++) gfxt_putchar('\xdb');
    rest -= seg;
  }
  for (int i = 0; i < rest; i++) gfxt_putchar('\xb0');
  for (int i = 0; i < n; i++) {
    gfxt_print(color[i]);
    gfxt_printf("%s %d%% - %dms\n", name[i], value[i] * 100 / total, value[i]);
  }
  gfxt_print(TERM_RESET "\n");
}

int cmd_top(const char *args) {
  int tw = 10;
  gfxt_get_size(&tw, NULL);
  bar_draw(tw, 4,
    "poll", TERM_YELLOW, gfx_step.poll,
    "draw", TERM_MAGENTA, gfx_step.draw,
    "work", TERM_GREEN, gfx_step.work,
    "sleep", TERM_BBLACK, gfx_step.sleep
  );
  bar_draw(tw, 2,
    "tick", TERM_CYAN, gfx_step.tick,
    "sleep", TERM_BBLACK, gfx_step.sleep
  );
  int e = gfx_step.tick - (gfx_step.draw + gfx_step.work + gfx_step.poll);
  e = e < 0 ? -e : e;
  gfxt_printf("e:%dms wait:%dms budget:%dms\n", e, gfx_step.wait, gfx_step.budget);
  gfxt_printf("%d/%dfps loop:%dms ", 1000 / (gfx_step.tick + gfx_step.sleep), GFX_FPS, gfx_step.tick);
  gfxt_printf("- #%d - %d gfx\n" TERM_RESET, gfx_step.frame, gfx_step.elm);
  return 0;
}

void gfxt_std_cmd_reg() {
  gfxt_register_cmd("sleep", "sleep for n seconds", cmd_sleep);
  gfxt_register_cmd("clear", "clear screen", cmd_clear);
  gfxt_register_cmd("echo", "print text", cmd_echo);
  gfxt_register_cmd("error", "print error message", cmd_error);
  gfxt_register_cmd("read", "read input", cmd_read);
  gfxt_register_cmd("eval", "evaluate command", cmd_eval);
  gfxt_register_cmd("time", "time command execution", cmd_time);
  gfxt_register_cmd("theme", "[id] set color theme", cmd_theme);
  gfxt_register_cmd("date", "current date/time", cmd_date);
  gfxt_register_cmd("beep", "[freq] [ms] beep", cmd_beep);
  gfxt_register_cmd("watch", "[s] [n] [cmd] run cmd periodically", cmd_watch);
  gfxt_register_cmd("help", "list available commands", cmd_help);
  gfxt_register_cmd("top", "gfx_step performance snapshot", cmd_top);
}
