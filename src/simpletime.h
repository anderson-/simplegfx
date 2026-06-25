#ifndef SIMPLETIME_H
#define SIMPLETIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GFX_TIME_OVERRIDE
#include <time.h>
static inline uint32_t gfx_time(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
#else
extern uint32_t gfx_time(void);
#endif

#ifndef GFX_SLEEP_OVERRIDE
#include <time.h>
static inline void gfx_sleep(int t) {
  if (t <= 0) return;
  struct timespec ts = { t / 1000, (t % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}
#else
extern void gfx_sleep(int t);
#endif

void gfx_run(void);
void gfx_wait(int ms);

extern int gfx_poll(void);
extern void gfx_flip(void);

typedef struct {
  uint8_t poll;
  uint8_t draw;
  uint8_t work;
  uint8_t sleep;

  uint8_t tick;
  uint8_t budget;
  uint16_t wait;
  uint8_t usage;

  uint32_t lfts;

  uint32_t elm;
  uint16_t frame;
} gfx_step_t;

static inline int gfx_ema(int avg, int last) {
  int diff = last - avg;
  if (diff == 0) return avg;
  int step = diff / 8;
  if (step == 0) step = diff > 0 ? 1 : -1;
  return avg + step;
}

static inline int gfx_dt(uint32_t start) {
  return (int)(gfx_time() - start);
}

#ifdef __cplusplus
}
#endif

#endif
