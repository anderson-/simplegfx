/* ══════════════════════════════════════════════════════════════════════════
 *  modtracker.c — MSM Tracker: editor/compositor de chiptune (4 canais)
 *
 *  Compilar e rodar:
 *    make sdl-modtracker-run        (ou macos-modtracker-run)
 *    make rg35xx-modtracker-deploy  (zip para o RG35xx)
 *
 *  Toda a aplicação vive em ext/audio/tracker_app.h, compartilhada com o
 *  exemplo do Cardputer (examples/gfx-modtracker). Controles na tela de
 *  ajuda (TAB) ou no topo do tracker_app.h.
 * ══════════════════════════════════════════════════════════════════════════ */

#include "simplegfx.h"
#include "tracker_app.h"
