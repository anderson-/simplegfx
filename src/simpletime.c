#include "simplegfx.h"
#include "simpletime.h"

gfx_step_t gfx_step;
int gfx_wait_frames = 4;

static int gfx_tick(int fast) {
  int framelen = (1000 / GFX_FPS);
  uint32_t t0 = gfx_time();
  if (gfx_poll()) return 1;
  uint32_t t1 = gfx_time();
  gfx_step.poll = gfx_ema(gfx_step.poll, t1 - t0);

  int elapsed = t1 - gfx_step.lfts;
  int target = framelen * gfx_wait_frames;
  if (elapsed >= target || !fast) {
    int save = elm;
    elm = 0;
    int dirty = gfx_draw(&gfx_step);
    if (dirty) {
      gfx_step.elm = elm;
      gfx_flip();
      gfx_clear();
      gfx_step.frame++;
    } else {
      elm = save;
    }
    gfx_step.draw = gfx_ema(gfx_step.draw, gfx_time() - t1);
    gfx_step.lfts = gfx_time(); // last frame time stamp
  }

  uint32_t t2 = gfx_time();
  gfx_step.budget = framelen - (t2 - t0);
  if (gfx_step.budget > framelen) gfx_step.budget = 0;
  gfx_process_data(&gfx_step);
  gfx_step.usage = gfx_ema(gfx_step.usage, framelen > 0 ? (gfx_step.tick * 100) / framelen : 0);
  if (gfx_step.usage > 99) gfx_step.usage = 99;
  uint32_t t3 = gfx_time();
  gfx_step.work = gfx_ema(gfx_step.work, t3 - t2);

  if (!fast) {
    uint32_t t4 = gfx_time();
    gfx_step.tick = gfx_ema(gfx_step.tick, t4 - t0);

    int remaining = framelen - (int)(t4 - t0);
    if (remaining > 0) {
      gfx_sleep(remaining);
    } else {
      remaining = 0;
    }
    gfx_step.sleep = gfx_ema(gfx_step.sleep, remaining);
  }

  return 0;
}

void gfx_run(void) {
  gfx_step = (gfx_step_t){0};
  while (1) if (gfx_tick(0)) break;
}

void gfx_wait(int ms) {
  uint32_t start = gfx_time();
  uint32_t timeout = start + ms;
  int framelen = (1000 / GFX_FPS);

  while (1) {
    uint32_t now = gfx_time();
    if (now >= timeout) break;
    int remaining = timeout - now;
    int elapsed = now - gfx_step.lfts;
    int target = framelen * gfx_wait_frames;
    if (remaining < gfx_step.tick || elapsed >= target) {
      if (gfx_tick(1)) return;
    }
  }

  gfx_step.wait = gfx_ema(gfx_step.wait, gfx_time() - start);
}
