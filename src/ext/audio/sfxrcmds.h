#pragma once

#include "simplegfx.h"
#include "ext/term/ansiutils.h"
#include "ext/term/simpleterm.h"
#include "ext/audio/sfxr.h"
#include "ext/audio/sfxr_presets.h"
#include <string.h>
#include <ctype.h>

static int is_base64_str(const char *s) {
  if (!s || strlen(s) != 24) return 0;
  for (int i = 0; i < 24; i++) {
    char c = s[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '+' || c == '/'))
      return 0;
  }
  return 1;
}

static int find_preset(const char *name) {
  if (!name || !name[0]) return -1;
  char *end;
  long n = strtol(name, &end, 10);
  if (*end == '\0' && n >= 0 && n < GFXA_SFXR_PRESET_COUNT)
    return (int)n;
  for (int i = 0; i < GFXA_SFXR_PRESET_COUNT; i++) {
    const char *pn = gfxa_sfxr_preset_name(i);
    if (!pn) continue;
    const char *a = name, *b = pn;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++; b++;
    }
    if (*a == '\0' && *b == '\0') return i;
    a = name; b = pn;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++; b++;
    }
    if (*a == '\0') return i;
  }
  return -1;
}

static void play_and_show(const float p[GFXA_SFXR_PARAM_COUNT]) {
  char b64[32];
  gfxa_sfxr_to_base64(p, b64, sizeof(b64));
  int c = gfxa_sfxr_play(p);
  if (c >= 0) {
    gfxt_printf("b64[%d]: " TERM_YELLOW "%s" TERM_RESET "\n", c, b64);
  } else {
    gfxt_printf(TERM_RED "failed" TERM_RESET "\n");
  }
}

int cmd_sfxr(const char *args) {
  if (!args || !args[0]) {
    float p[GFXA_SFXR_PARAM_COUNT];
    gfxa_sfxr_random(p);
    play_and_show(p);
    return 0;
  }

  if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
    gfxt_println(TERM_CYAN "SFXR presets:" TERM_RESET);
    for (int i = 0; i < GFXA_SFXR_PRESET_COUNT; i++) {
      gfxt_printf("  %d  %s\n", i, gfxa_sfxr_preset_name(i));
    }
    gfxt_printf("  %d  Random\n", GFXA_SFXR_PRESET_COUNT);
    gfxt_printf("  %d  Mutate (mutate last random)\n", GFXA_SFXR_PRESET_COUNT + 1);
    gfxt_printf(TERM_CYAN "Usage:" TERM_RESET "\n"
      "  sfxr              random\n"
      "  sfxr <N|name>     preset\n"
      "  sfxr <base64>     from 24-char string\n"
      "  sfxr list         this list\n");
    return 0;
  }

  int idx = find_preset(args);
  if (idx >= 0) {
    float p[GFXA_SFXR_PARAM_COUNT];
    gfxa_sfxr_preset(idx, p);
    play_and_show(p);
    return 0;
  } else if (is_base64_str(args)) {
    float p[GFXA_SFXR_PARAM_COUNT];
    if (gfxa_sfxr_from_base64(args, p) == 0) {
      gfxa_sfxr_play(p);
      return 0;
    }
  }

  gfxt_println(TERM_RED "sfxr: unknown argument. try 'sfxr list'" TERM_RESET);
  return 1;
}

void gfxt_sfxr_cmd_reg(void) {
  gfxt_register_cmd("sfxr", "[preset|base64|list] play SFXR sound", cmd_sfxr);
}
