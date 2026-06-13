#include "simplegfx.h"
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>

#if defined(GFX_HEADLESS)

static int exit_app = 0;
static int term_saved = 0;
static struct termios old_term;

static void signal_handler(int sig) {
  (void)sig;
  exit_app = 1;
}

static void headless_sleep_ms(int ms) {
  struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}

static void term_restore(void) {
  if (term_saved) tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
  term_saved = 0;
}

static void term_rawish(void) {
  if (!isatty(STDIN_FILENO)) return;
  if (tcgetattr(STDIN_FILENO, &old_term) != 0) return;
  struct termios t = old_term;
  t.c_lflag &= ~(ICANON | ECHO);
  t.c_cc[VMIN] = 0;
  t.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0) term_saved = 1;
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
  term_rawish();
  gfx_yeld = headless_loop;
  gfx_app(1);

  while (!exit_app) {
    headless_loop();
  }

  gfx_app(0);
  term_restore();
  gfx_cleanup();
  return 0;
}

#endif
