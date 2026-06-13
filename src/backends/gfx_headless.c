#include "simplegfx.h"
#include <signal.h>
#include <time.h>

#if defined(GFX_HEADLESS)

static int exit_app = 0;

static void signal_handler(int sig) {
  (void)sig;
  exit_app = 1;
}

static void headless_sleep_ms(int ms) {
  struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}

static void headless_loop(void) {
  gfx_process_data(0);
  if (gfx_draw(GFX_FPS)) {
    gfx_clear();
  }
  headless_sleep_ms(1000 / GFX_FPS);
}

int main(void) {
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGHUP, signal_handler);

  if (gfx_setup() != 0) return 1;
  gfx_set_font(&font5x7);
  gfx_yeld = headless_loop;
  gfx_app(1);

  while (!exit_app) {
    headless_loop();
  }

  gfx_app(0);
  gfx_cleanup();
  return 0;
}

#endif
