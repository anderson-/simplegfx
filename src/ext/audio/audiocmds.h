#pragma once

#include "simplegfx.h"
#include "ext/term/simpleterm.h"
#include "ext/audio/rtttl.h"

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

void gfxt_audio_cmd_reg(void) {
  gfxt_register_cmd("play", "[rtttl] play RTTTL melody", cmd_play);
}
