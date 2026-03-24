#include "simplegfx.h"

#ifdef GFX_BUFFER

static uint16_t currentColor = 0xFFFF;
static uint16_t frameBuffer[WINDOW_WIDTH * WINDOW_HEIGHT];

uint16_t* gfx_get_frame_buffer(void) {
  return frameBuffer;
}

int gfx_setup(void) {
  return 0;
}

void gfx_cleanup(void) {
}

void gfx_set_color(int r, int g, int b) {
  uint16_t rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  currentColor = (uint16_t)((rgb << 8) | (rgb >> 8));
}

void gfx_point(int x, int y) {
  if (x < 0 || y < 0 || x >= WINDOW_WIDTH || y >= WINDOW_HEIGHT) return;
  frameBuffer[y * WINDOW_WIDTH + x] = currentColor;
  elm++;
}

void gfx_fill_rect(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  int x0 = (x < 0) ? 0 : x;
  int x1 = (x + w - 1 > WINDOW_WIDTH - 1) ? WINDOW_WIDTH - 1 : x + w - 1;
  int y0 = (y < 0) ? 0 : y;
  int y1 = (y + h - 1 > WINDOW_HEIGHT - 1) ? WINDOW_HEIGHT - 1 : y + h - 1;
  for (int py = y0; py <= y1; ++py)
    for (int px = x0; px <= x1; ++px)
      frameBuffer[py * WINDOW_WIDTH + px] = currentColor;
  elm++;
}

void gfx_clear(void) {
  memset(frameBuffer, 0, sizeof(frameBuffer));
  elm = 0;
}

void gfx_run(void) {
  gfx_app(1);
}

void gfx_screenshot(const char * filename) {
}

void gfx_delay(int ms) {
}

#endif
