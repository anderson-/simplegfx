#include "simplegfx.h"
#include "simpletime.h"

gfx_step_t gfx_step;
int gfx_wait_sleep = 4;

static int gfx_tick(gfx_step_t *s, uint32_t last) {
  uint32_t t0 = gfx_time();
  s->dt = (int)(t0 - last);

  if (gfx_poll()) return 1;
  uint32_t t1 = gfx_time();

  s->budget = (1000 / GFX_FPS) - s->draw - s->proc;
  if (s->budget < 0) s->budget = 0;

  gfx_process_data(s);
  uint32_t t2 = gfx_time();
  s->proc = gfx_ema(s->proc, (int)(t2 - t1));

  int save = elm;
  elm = 0;
  int dirty = gfx_draw(s);
  uint32_t t3;
  if (dirty) {
    s->elm = elm;
    uint32_t prev_flip = s->last_frame_ts;
    gfx_flip();
    gfx_clear();
    t3 = gfx_time();
    s->draw = gfx_ema(s->draw, (int)(t3 - t2));
    if (prev_flip) s->flip = gfx_ema(s->flip, (int)(t3 - prev_flip));
    s->last_frame_ts = t3;
    s->frame++;
  } else {
    elm = save;
    t3 = gfx_time();
  }
  s->loop = gfx_ema(s->loop, (int)(t3 - t0));
  return 0;
}

void gfx_run(void) {
  gfx_step = (gfx_step_t){0};
  uint32_t last = gfx_time();
  while (1) {
    uint32_t t0 = gfx_time();
    if (gfx_tick(&gfx_step, last)) break;
    int sleep_ms = (1000 / GFX_FPS) - (int)(gfx_time() - t0);
    if (sleep_ms > 0) gfx_sleep(sleep_ms);
    last = t0;
  }
}

void gfx_wait(int ms) {
  uint32_t deadline = gfx_time() + (uint32_t)ms;
  int frame_period = 1000 / GFX_FPS;
  gfx_step_t local;
  uint32_t last = gfx_time();

  while (gfx_time() < deadline) {
    int remaining = (int)(deadline - gfx_time());
    if (remaining <= 0) break;

    uint32_t now = gfx_time();
    int since = (int)(now - gfx_step.last_frame_ts);
    int desired = frame_period * gfx_wait_sleep;

    if (since >= desired) {
      local = gfx_step;
      gfx_tick(&local, last);
      gfx_step.last_frame_ts = local.last_frame_ts;
      last = gfx_time();
      continue;
    }

    int sleep_ms = desired - since;
    if (sleep_ms > remaining) sleep_ms = remaining;
    if (sleep_ms > 1) gfx_sleep(sleep_ms);

    now = gfx_time();
    since = (int)(now - gfx_step.last_frame_ts);
    if (since >= desired) {
      local = gfx_step;
      gfx_tick(&local, last);
      gfx_step.last_frame_ts = local.last_frame_ts;
      last = now;
    }
  }
}
