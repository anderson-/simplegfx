#include <simplegfx.h>

#ifdef USE_SDL2
SDL_Renderer * renderer = NULL;
SDL_Window * window = NULL;
#else
SDL_Surface * screen = NULL;
uint32_t color = 0;
#endif

static font_t * _font = NULL;
static unsigned int seed = 0;
extern double volume;
uint32_t elm = 0;
char * printf_buf = NULL;
int printf_size = 0;
int printf_len = 0;
int full_kb = 0;

int main(int argv, char** args) {
  if (gfx_setup() != 0) {
    return 1;
  }
  gfx_set_font(&font5x7);
  gfx_run();
  gfx_app(0);
  gfx_cleanup();
  return 0;
}

int gfx_setup(void) {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("SDL2 Initialization Error: %s\n", SDL_GetError());
    return 1;
  }

#ifdef USE_SDL2
  window = SDL_CreateWindow("SDL",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            WINDOW_WIDTH, WINDOW_HEIGHT,
                            SDL_WINDOW_SHOWN);
  if (!window) {
    printf("SDL2 Create Window Error: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
#ifdef SCREENSHOT
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
#else
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#endif
  if (!renderer) {
    printf("SDL2 Create Renderer Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
#else
  screen = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT,
                            32, SDL_HWSURFACE | SDL_DOUBLEBUF);
  if (!screen) {
    printf("SDL1.2 Set Video Mode Error: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_WM_SetCaption("SDL", NULL);
#endif
  SDL_ShowCursor(SDL_DISABLE);
  seed = (unsigned int)time(NULL);

  if (audio_setup() != 0) {
    printf("Audio setup failed\n");
    return 1;
  }

  return 0;
}

void gfx_cleanup(void) {
#ifdef USE_SDL2
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
#else
  SDL_FreeSurface(screen);
#endif
  audio_cleanup();
  gfx_clear_text_buffer();
  SDL_Quit();
}

void gfx_run(void) {
  uint32_t delay = 1000 / FPS;
  uint32_t start;
  uint32_t busytime = 0;
  float fps = FPS;
  SDL_Event event;

  gfx_app(1);

  while (1) {
    start = SDL_GetTicks();

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        return;
      } else if (event.type == SDL_KEYDOWN) {
        if (gfx_on_key(event.key.keysym.sym, 1) != 0) {
          return;
        } else if (event.key.keysym.sym == BTN_POWER) {
          printf("Exit key pressed\n");
          return;
        } else if (full_kb && event.key.keysym.sym == BTN_EXIT) {
          printf("Exit key pressed\n");
          return;
        } else if (event.key.keysym.sym == BTN_VOLUME_UP) {
          volume *= 2;
          if (volume > 0.5) volume = 0.5;
        } else if (event.key.keysym.sym == BTN_VOLUME_DOWN) {
          volume /= 2;
          if (volume <= 0.05) volume = 0.05;
        } else if (event.key.keysym.sym == 92) {
          gfx_screenshot("build/screenshot.bmp");
        }
      } else if (event.type == SDL_KEYUP) {
        if (gfx_on_key(event.key.keysym.sym, 0) != 0) {
          return;
        }
      }
#if defined(USE_SDL2) && defined(SCREENSHOT)
      if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        gfx_screenshot("build/screenshot.bmp");
        return;
      }
#endif
    }

    gfx_clear();
    gfx_draw(fps);
#ifdef USE_SDL2
    SDL_RenderPresent(renderer);
#else
    SDL_Flip(screen);
#endif

#if !defined(USE_SDL2) && defined(SCREENSHOT)
    if (SDL_GetAppState() & SDL_APPACTIVE) {
      gfx_screenshot("build/screenshot.bmp");
      return;
    }
#endif

    busytime = SDL_GetTicks() - start;
    if (delay > busytime) {
      while (1) {
        gfx_process_data(delay - busytime);
        busytime = SDL_GetTicks() - start;
        if (busytime >= delay) {
          break;
        }
      }
    }

    float frame_time = (SDL_GetTicks() - start) / 1000.0f;
    if (frame_time > 0) {
      float current_fps = 1.0f / frame_time;
      fps = (0.1f * current_fps) + (0.9f * fps);
    }
  }
}

void gfx_point(int x, int y) {
#ifdef USE_SDL2
  SDL_RenderDrawPoint(renderer, x, y);
#else
  ((Uint32 *)screen->pixels)[y * screen->w + x] = color;
#endif
  elm++;
}

void gfx_set_color(int r, int g, int b) {
#ifdef USE_SDL2
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
#else
  color = SDL_MapRGB(screen->format, r, g, b);
#endif
}

void gfx_set_font(font_t * font) {
  _font = font;
  if (_font == NULL) {
    _font = &font5x7;
  }
  if (_font->count == 0) {

    uint8_t * data = _font->data;
    for (int i = 0; i < 512 * _font->width; i++) {
      if (data[i] == 1 && data[i + 1] == 2
          && data[i + 2] == 3 && data[i + 3] == 4
          && data[i + 4] == 5) {
        _font->count = i / _font->width;
        printf("gliphs: %d\n", _font->count);
        break;
      }
    }
  }
}

font_t * gfx_get_font(void) {
  return _font;
}

int gfx_text(const char * text, int x, int y, int size) {
  if (text == NULL) {
    return 0;
  }
  int cx = x;
  int cy = y;
  font_t f = *_font;
  int fheight = f.height;
  int fwidth = f.width;
  cy += fheight * size;
  int len = (int)strlen(text);
  for (int i = 0; i < len; i++) {
    uint8_t C = text[i];
    if (C == '\r' && text[i + 1] == '\n') {
      i++;
      cx = x;
      cy += fheight * size + size;
      if (cy > WINDOW_HEIGHT) {
        cy = y;
      }
      continue;
    }
    for (int c = 0; c < fwidth; c++) {
      for (uint8_t l = 0; l < fheight; l++) {
        uint8_t mask = 1 << (fheight - l - 1);
        if (f.data[C * fwidth + c] & mask) {
          if (size == 1) {
            gfx_point(cx + c, cy - l);
          } else {
            gfx_fill_rect(cx + c * size, cy - l * size, size, size);
          }
        }
      }
    }
    cx += fwidth * size + size;
    if (cx > WINDOW_WIDTH) {
      cx = x;
      cy += fheight * size + size;
    }
  }
  cy += fheight * size + size;
  return cy;
}

int gfx_clear_text_buffer(void) {
  if (printf_buf != NULL) {
    free(printf_buf);
    printf_buf = NULL;
  }
  printf_size = 0;
  printf_len = 0;
  return 0;
}

int gfx_printf(const char * format, ...) {
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len < 0) {
    return -1;
  }
  if (len + printf_len >= printf_size) {
    printf_size = (len + printf_len + 64) * 2;
    char * new_buf = (char *)realloc(printf_buf, printf_size);
    if (new_buf == NULL) {
      return -1;
    }
    printf_buf = new_buf;
  }
  va_start(args, format);
  vsnprintf(printf_buf + printf_len, printf_size - printf_len, format, args);
  va_end(args);
  printf_len += len;
  return len;
}

int gfx_font_table(int x, int y, int size) {
  font_t f = *_font;
  char t[2] = {0, 0};
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 32; j++) {
      t[0] = i * 32 + j;
      if (t[0] >= f.count) break;
      gfx_text(t, x + j * (f.width * size + size),
               y + i * (f.height * size + size), size);
    }
  }
  return y + 8 * (f.height * size + size);
}

void gfx_clear(void) {
#ifdef USE_SDL2
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
#else
  SDL_FillRect(screen, NULL, 0);
#endif
  elm = 0;
}

void gfx_fill_rect(int x, int y, int w, int h) {
#ifdef USE_SDL2
  SDL_Rect rect = { x, y, w, h };
  SDL_RenderFillRect(renderer, &rect);
#else
  SDL_Rect rect = { x, y, w, h };
  SDL_FillRect(screen, &rect, color);
#endif
  elm++;
}

int gfx_fast_rand(void) {
  seed = seed * 1103515245 + 12345;
  return (unsigned int)(seed / 65536) % 32768;
}

void gfx_screenshot(const char * filename) {
  printf("Saving screenshot\n");
#ifdef USE_SDL2
  SDL_Surface * s = SDL_CreateRGBSurface(0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, 0, 0, 0, 0);
  SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
  SDL_SaveBMP(s, filename);
  SDL_FreeSurface(s);
#else
  SDL_SaveBMP(screen, filename);
#endif
}

void gfx_delay(int ms) {
  SDL_Delay(ms);
}