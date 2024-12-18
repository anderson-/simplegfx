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

int main(int argv, char** args) {
  if (gfx_setup() != 0) {
    return 1;
  }
  gfx_set_font(&font5x7);
  gfx_run();
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

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
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
  SDL_Quit();
}

void gfx_run(void) {
  uint32_t delay = 1000 / FPS;
  uint32_t start;
  uint32_t busytime;
  SDL_Event event;

  while (1) {
    start = SDL_GetTicks();

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        return;
      } else if (event.type == SDL_KEYDOWN) {
        if (on_key(event.key.keysym.sym, 1) != 0) {
          return;
        } else if (event.key.keysym.sym == BTN_POWER
                || event.key.keysym.sym == 27) {
          printf("Exit key pressed\n");
          return;
        } else if (event.key.keysym.sym == BTN_VOLUME_UP) {
          volume *= 2;
          if (volume > 0.5) volume = 0.5;
        } else if (event.key.keysym.sym == BTN_VOLUME_DOWN) {
          volume /= 2;
          if (volume <= 0.05) volume = 0.05;
        }
      } else if (event.type == SDL_KEYUP) {
        if (on_key(event.key.keysym.sym, 0) != 0) {
          return;
        }
      }
    }

    gfx_clear();
    render(delay - busytime);
#ifdef USE_SDL2
    SDL_RenderPresent(renderer);
#else
    SDL_Flip(screen);
#endif

    busytime = SDL_GetTicks() - start;
    if (delay > busytime) {
      SDL_Delay(delay - busytime);
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

void gfx_text(const char * text, int x, int y, int size) {
  int cx = x;
  font_t f = *_font;
  y += f.height * size;
  for (int i = 0; i < (int)strlen(text); i++) {
    uint8_t C = text[i];
    for (int c = 0; c < f.width; c++) {
      for (uint8_t l = 0; l < f.height; l++) {
        uint8_t mask = 1 << (f.height - l - 1);
        if (f.data[C * f.width + c] & mask) {
          if (size == 1) {
            gfx_point(cx + c, y - l);
          } else {
            gfx_fill_rect(cx + c * size, y - l * size, size, size);
          }
        }
      }
    }
    cx += f.width * size + size;
  }
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

int fast_rand(void) {
  seed = seed * 1103515245 + 12345;
  return (unsigned int)(seed / 65536) % 32768;
}
