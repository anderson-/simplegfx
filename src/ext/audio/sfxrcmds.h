#pragma once

#include "simplegfx.h"
#include "ext/term/ansiutils.h"
#include "ext/term/simpleterm.h"
#include "ext/audio/sfxr.h"
#include "ext/audio/sfxr_presets.h"
#include <string.h>
#include <ctype.h>

/* ── helpers ─────────────────────────────────────────────────────────────── */

/* Retorna 1 se a string tem exatamente 24 chars e só chars base64 válidos */
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

/* Case-insensitive match de nome de preset; retorna índice ou -1 */
static int find_preset(const char *name) {
  if (!name || !name[0]) return -1;
  /* tenta número */
  char *end;
  long n = strtol(name, &end, 10);
  if (*end == '\0' && n >= 0 && n < GFXA_SFXR_PRESET_COUNT)
    return (int)n;
  /* tenta match case-insensitive no nome */
  for (int i = 0; i < GFXA_SFXR_PRESET_COUNT; i++) {
    const char *pn = gfxa_sfxr_preset_name(i);
    if (!pn) continue;
    const char *a = name, *b = pn;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++; b++;
    }
    if (*a == '\0' && *b == '\0') return i;
    /* tampa match parcial tipo "pickup" vs "Pickup/Coin" */
    a = name; b = pn;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++; b++;
    }
    if (*a == '\0') return i;
  }
  return -1;
}

/* Toca e printa o base64 */
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

/* ── cmd_sfxr ────────────────────────────────────────────────────────────── */

int cmd_sfxr(const char *args) {
  if (!args || !args[0]) {
    /* sem args → random */
    float p[GFXA_SFXR_PARAM_COUNT];
    gfxa_sfxr_random(p);
    play_and_show(p);
    return 0;
  }

  /* subcomandos */
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
      "  sfxr random       random\n"
      "  sfxr mutate       mutate\n"
      "  sfxr encode <arg> show base64 without playing\n"
      "  sfxr list         this list\n");
    return 0;
  }

  /* encode <arg> — mostra base64 sem tocar */
  if (strncmp(args, "encode ", 7) == 0) {
    const char *what = args + 7;
    float p[GFXA_SFXR_PARAM_COUNT];
    char b64[32];

    if (strcmp(what, "random") == 0) {
      gfxa_sfxr_random(p);
    } else {
      int idx = find_preset(what);
      if (idx >= 0) {
        gfxa_sfxr_preset(idx, p);
      } else if (is_base64_str(what)) {
        gfxt_println(TERM_YELLOW "already base64" TERM_RESET);
        return 0;
      } else {
        gfxt_println(TERM_RED "unknown preset" TERM_RESET);
        return 1;
      }
    }
    gfxa_sfxr_to_base64(p, b64, sizeof(b64));
    gfxt_printf("%s\n", b64);
    return 0;
  }

  /* random */
  if (strcmp(args, "random") == 0 || strcmp(args, "rand") == 0) {
    float p[GFXA_SFXR_PARAM_COUNT];
    gfxa_sfxr_random(p);
    play_and_show(p);
    return 0;
  }

  /* mutate */
  if (strcmp(args, "mutate") == 0 || strcmp(args, "mut") == 0) {
    static float last[GFXA_SFXR_PARAM_COUNT];
    static int has_last = 0;
    if (!has_last) {
      gfxa_sfxr_random(last);
      has_last = 1;
    }
    gfxa_sfxr_mutate(last);
    play_and_show(last);
    return 0;
  }

  /* Tenta preset por nome/índice */
  {
    int idx = find_preset(args);
    if (idx >= 0) {
      float p[GFXA_SFXR_PARAM_COUNT];
      gfxa_sfxr_preset(idx, p);
      play_and_show(p);
      return 0;
    }
  }

  /* Tenta base64 */
  if (is_base64_str(args)) {
    float p[GFXA_SFXR_PARAM_COUNT];
    if (gfxa_sfxr_from_base64(args, p) == 0) {
      gfxa_sfxr_play(p);
      return 0;
    }
  }

  gfxt_println(TERM_RED "sfxr: unknown argument. try 'sfxr list'" TERM_RESET);
  return 1;
}

/* ── Registro ────────────────────────────────────────────────────────────── */

int cmd_testsfxr(const char *args) {
  return gfxt_run_cmd("watch 0.03 10 sfxr piano");
}

void gfxt_sfxr_cmd_reg(void) {
  gfxt_register_cmd("sfxr", "[preset|base64|random|list] play SFXR sound", cmd_sfxr);
  gfxt_register_cmd("testsfxr", "test SFXR mix", cmd_testsfxr);
}
