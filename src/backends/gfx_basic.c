#include "simplegfx.h"

#ifdef GFX_BASIC

static int currentColorR, currentColorG, currentColorB;

int gfx_setup(void) { return 0; }
void gfx_cleanup(void) { }
void gfx_clear(void) { elm = 0; }
void gfx_set_color(int r, int g, int b) {
    currentColorR = r; currentColorG = g; currentColorB = b;
}
void gfx_point(int x, int y) { elm++; }
void gfx_fill_rect(int x, int y, int w, int h) { elm++; }
void gfx_run(void) {
    gfx_app(1);
    gfx_draw(GFX_FPS);
}
void gfx_screenshot(const char * filename) { }
void gfx_delay(int ms) { }

#endif
