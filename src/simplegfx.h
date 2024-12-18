#ifndef SIMPLEGFX_H
#define SIMPLEGFX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINDOW_WIDTH
#define WINDOW_WIDTH 640
#endif

#ifndef WINDOW_HEIGHT
#define WINDOW_HEIGHT 480
#endif

#ifndef FPS
#define FPS 30
#endif

#ifdef USE_SDL2
  #include <SDL2/SDL.h>
  #define FONT_COLOR SDL_Color

  extern SDL_Renderer * renderer;
  extern SDL_Window * window;
#else
  #include <SDL/SDL.h>
  #undef main

  extern SDL_Surface * screen;
#endif

#include <time.h>
#include <simplefont.h>
#include <font5x7.h>

int gfx_setup(void);
void gfx_cleanup(void);
void gfx_point(int x, int y);
void gfx_set_color(int r, int g, int b);
void gfx_set_font(font_t * font);
font_t * gfx_get_font(void);
int gfx_font_table(int x, int y, int size);
void gfx_text(const char * text, int x, int y, int size);
void gfx_clear(void);
void gfx_fill_rect(int x, int y, int w, int h);
void gfx_run(void);

extern void render(void);
extern int on_key(char key, int down);

#ifdef __cplusplus
}
#endif

#endif