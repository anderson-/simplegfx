#include "simplemath.h"

unsigned int _seed = 12345;

int gfx_fast_rand(void) {
  _seed = _seed * 1103515245 + 12345;
  return (int)((_seed / 65536) % 32768);
}

int32_t gfx_fast_isin(int32_t phase) {
  uint32_t p = (uint32_t)phase & 0xFFFF;
  uint32_t neg = p >> 15;
  if (neg) p = 65536u - p;
  uint32_t y = p * (32768u - p);
  y = (y + 4096u) >> 13;
  if (y > 32767u) y = 32767u;
  return neg ? -(int32_t)y : (int32_t)y;
}

int32_t gfx_fast_icos(int32_t phase) {
  return gfx_fast_isin(phase + 16384);
}
