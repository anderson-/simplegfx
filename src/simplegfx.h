#ifndef SIMPLEGFX_H
#define SIMPLEGFX_H

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if __has_include("gfx_config.h")
#include "gfx_config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINDOW_WIDTH
#define WINDOW_WIDTH 640
#endif

#ifndef WINDOW_HEIGHT
#define WINDOW_HEIGHT 480
#endif

#ifndef GFX_FPS
#define GFX_FPS 30
#endif

#include <time.h>
#include <math.h>
#include "keymap.h"
#include "simplefont.h"

#ifndef M_PI
#define M_PI 3.14159265
#endif

// Common variables
extern uint32_t elm;
extern char * printf_buf;
extern int printf_size;
extern int printf_len;
extern int full_kb;

// API functions
int gfx_setup(void);
void gfx_cleanup(void);
void gfx_run(void);
void gfx_clear(void);
void gfx_set_color(int r, int g, int b);
void gfx_point(int x, int y);
void gfx_fill_rect(int x, int y, int w, int h);

// Text & Font functions
void gfx_set_font(font_t * font);
font_t * gfx_get_font(void);
int gfx_text(const char * text, int x, int y, int size);
int gfx_font_table(int x, int y, int size);
int gfx_clear_text_buffer(void);
int gfx_printf(const char * format, ...);

// Utils
int gfx_fast_rand(void);
void gfx_screenshot(const char * filename);
void gfx_delay(int ms);

// Backend specific
#if defined(GFX_BUFFER) || !(defined(GFX_SDL) || defined(GFX_SDL2))
uint16_t* gfx_get_frame_buffer(void);
#endif

#if defined(GFX_SDL) || defined(GFX_SDL2)
#ifdef GFX_SDL2
  #include <SDL2/SDL.h>
  extern SDL_Renderer * renderer;
  extern SDL_Window * window;
#else
  #include <SDL/SDL.h>
  extern SDL_Surface * screen;
#endif
void beep(int freq, int ms);
extern double volume;
#endif

// Callbacks to be implemented by the user app
extern void gfx_app(int init);
extern void gfx_draw(float fps);
extern int gfx_on_key(char key, int down);
extern void gfx_process_data(int compute_time);

#ifdef __cplusplus
}
#endif

#endif