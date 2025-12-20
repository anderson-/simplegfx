#include <simplegfx.h>
#include <world3d.h>

static w3_state world;

void gfx_app(int init) {
  if (init) {
    w3_init(&world);
  }
}

void gfx_process_data(int compute_time) {
  (void)compute_time;
}

void gfx_draw(float fps) {
  w3_draw(&world, fps);
}

int gfx_on_key(char key, int down) {
  return w3_handle_key(&world, key, down);
}
