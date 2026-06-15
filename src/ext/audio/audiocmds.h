#pragma once

#include "simplegfx.h"
#include "ext/term/simpleterm.h"
#include "rtttl.h"
#include "sfxr.h"
#include "presets.h"
#include "ext/term/b64.h"

int cmd_play(const char *args) {
  if (!args || !args[0]) {
    gfxa_rtttl_play("scale_up:d=32,o=5,b=100:c,c#,d#,e,f#,g#,a#,b", NULL);
    return 0;
  }
  int result = gfxa_rtttl_play(args, NULL);
  if (result != 0) {
    gfx_beep(880, 120);
    gfxt_println(TERM_RED "invalid RTTTL format" TERM_RESET);
  }
  return result;
}

static afx_t s_last;

int cmd_sfxr(const char *args) {
  int mutate = 0;
  char a[64];
  int ai = 0;

  if (args) {
    for (int i = 0; args[i] && ai < (int)sizeof(a) - 1; i++) {
      if (args[i] == '-' && args[i + 1] == 'm') {
        mutate = 1;
        i++;
        if (args[i + 1] == ' ') i++;
        continue;
      }
      a[ai++] = args[i];
    }
    a[ai] = '\0';
    while (ai > 0 && a[ai - 1] == ' ') a[--ai] = '\0';
  }

  afx_t *p = &s_last;

  if (a[0]) {
    char *end;
    long n = strtol(a, &end, 10);
    if (*end == '\0' && n >= 0 && n < GFXA_SFXR_PRESET_COUNT) {
      gfxa_sfxr_preset((int)n, p);
    } else if (base64_decode(a, (uint8_t *)p, sizeof(afx_t)) != sizeof(afx_t)) {
      gfxt_println(TERM_RED "sfxr: expected preset 0-8 or 32-char base64" TERM_RESET);
      return 1;
    }
    if (mutate) gfxa_sfxr_mutate(p);
  } else if (mutate) {
    gfxa_sfxr_mutate(p);
  } else {
    gfxa_sfxr_random(p);
  }

  sfxr_state_t *s = gfxa_sfxr_play(-1, p, -1);
  if (!s) {
    gfxt_println(TERM_RED "sfxr: no free slots" TERM_RESET);
    return 1;
  }

  char b64[33];
  base64_encode((const uint8_t *)p, sizeof(afx_t), b64, sizeof(b64));
  gfxt_printf("%s" TERM_GREEN "@%d" TERM_RESET "\n", b64, s->audio_ch);
  return 0;
}

void gfxt_audio_cmd_reg(void) {
  gfxt_register_cmd("play", "[rtttl] play RTTTL melody", cmd_play);
  gfxt_register_cmd("sfxr", "[N|b64] [-m] play sfxr sound (opt mutate)", cmd_sfxr);
}
