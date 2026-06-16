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
uint32_t gfx_time(void);
#endif

#ifndef GFX_SLEEP_OVERRIDE
#include <time.h>
static inline void gfx_sleep(int t) {
  if (t <= 0) return;
  struct timespec ts = { t / 1000, (t % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}
#else
void gfx_sleep(int t);
#endif

int gfx_poll(void);
void gfx_flip(void);

typedef struct {
  int dt;
  int draw;
  int proc;
  int loop;
  int budget;
  int elm;
  int frame;
  int flip;
  uint32_t last_frame_ts;
} gfx_step_t;

extern gfx_step_t gfx_step;
extern int gfx_wait_sleep;

static inline int gfx_ema(int avg, int last) {
  return avg + ((last - avg) >> 3);
}

static inline int gfx_dt(uint32_t start) {
  return (int)(gfx_time() - start);
}

void gfx_run(void);
void gfx_wait(int ms);

#ifdef __cplusplus
}
#endif

#endif
