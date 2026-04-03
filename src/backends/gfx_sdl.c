#include "simplegfx.h"
#include <signal.h>

#if defined(GFX_SDL) || defined(GFX_SDL2)

static double phase = 0.0;
static int playing = 0;
static int samples = 0;
static int samplesc = 0;
static int frequency = 0;
static int sr = 16000;
double volume = 0.2;
static int fadein = 256;
static int fadeout = 256;
static int exit_app = 0;

void signal_handler(int sig) {
  exit_app = 1;
}

#undef main
int main(int argc, char* argv[]) {
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGHUP, signal_handler);
  if (gfx_setup() != 0) {
    return 1;
  }
  gfx_set_font(&font5x7);
  gfx_run();
  gfx_app(0);
  gfx_cleanup();
  return 0;
}

void audio_callback(void * userdata, uint8_t * stream, int len) {
  int16_t * buffer = (int16_t *)stream;
  int genlen = len / sizeof(int16_t);
  for (int i = 0; i < genlen; i++) {
  buffer[i] = 0;
  }
  if (!playing) {
  SDL_PauseAudio(1);
  return;
  }
  if (samplesc + genlen > samples) {
  genlen = samples - samplesc;
  if (genlen < 0) genlen = 0;
  }
  for (int i = 0; i < genlen; i++) {
  double vfact = 1.0;
  if (samplesc < fadein) {
    vfact = (double)samplesc / (double)fadein;
  } else if (samples - samplesc < fadeout) {
    vfact = (double)(samples - samplesc) / (double)fadeout;
  }
  double s = sin(2.0 * M_PI * phase) * (volume * vfact);
  double amp = s * 32767.0;
  if (amp > 32767.0) amp = 32767.0;
  if (amp < -32768.0) amp = -32768.0;
  buffer[i] = (int16_t) amp;
  phase += frequency * 1.0 / sr;
  if (phase >= 1.0) phase -= 1.0;
  samplesc++;
  }
  if (samples > 0 && samplesc >= samples) {
  playing = 0;
  }
}

static int audio_setup(void) {
  SDL_AudioSpec spec;
  SDL_memset(&spec, 0, sizeof(spec));
  spec.freq = sr;
  spec.format = AUDIO_S16;
  spec.channels = 1;
  spec.samples = 2048;
  spec.callback = audio_callback;
  spec.userdata = NULL;
  if (SDL_OpenAudio(&spec, NULL) < 0) {
  fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
  return 1;
  }
  SDL_PauseAudio(0);
  return 0;
}

static void audio_cleanup(void) {
  SDL_CloseAudio();
}

void beep(int freq, int ms) {
  SDL_LockAudio();
  frequency = (double)freq;
  samples = (int)((ms / 1000.0) * sr);
  if (samples < fadein + fadeout + 10) {
  samples = fadein + fadeout + 10;
  }
  if (!playing) {
  phase = 0.0;
  samplesc = 0;
  playing = 1;
  SDL_PauseAudio(0);
  } else {
  samplesc = fadein;
  }
  SDL_UnlockAudio();
}

#ifdef GFX_SDL2
SDL_Renderer * renderer = NULL;
SDL_Window * window = NULL;
#else
SDL_Surface * screen = NULL;
static uint32_t color = 0;
#endif

int gfx_setup(void) {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("SDL Initialization Error: %s\n", SDL_GetError());
    return 1;
  }

#ifdef GFX_SDL2
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

  if (audio_setup() != 0) {
    printf("Audio setup failed\n");
    return 1;
  }

  return 0;
}

void gfx_cleanup(void) {
#ifdef GFX_SDL2
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
#else
  if (screen) SDL_FreeSurface(screen);
#endif
  audio_cleanup();
  gfx_clear_text_buffer();
  SDL_Quit();
}

void gfx_clear(void) {
#ifdef GFX_SDL2
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
#else
  SDL_FillRect(screen, NULL, 0);
#endif
  elm = 0;
}

void gfx_set_color(int r, int g, int b) {
#ifdef GFX_SDL2
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
#else
  color = SDL_MapRGB(screen->format, r, g, b);
#endif
}

void gfx_point(int x, int y) {
#ifdef GFX_SDL2
  SDL_RenderDrawPoint(renderer, x, y);
#else
  if (x >= 0 && x < screen->w && y >= 0 && y < screen->h) {
    ((Uint32 *)screen->pixels)[y * screen->w + x] = color;
  }
#endif
  elm++;
}

void gfx_fill_rect(int x, int y, int w, int h) {
  SDL_Rect rect = { x, y, w, h };
#ifdef GFX_SDL2
  SDL_RenderFillRect(renderer, &rect);
#else
  SDL_FillRect(screen, &rect, color);
#endif
  elm++;
}

static uint32_t delay = 1000 / GFX_FPS;
static uint32_t start;
static uint32_t busytime = 0;
static float fps = GFX_FPS;
static SDL_Event event;
static int pwrclicks = 0;
static int pwrdown = 0;

static void loop() {
  start = SDL_GetTicks();
  while (SDL_PollEvent(&event)) {
    char key = (char)event.key.keysym.sym;
    if (event.type == SDL_QUIT) {
      exit(0);
      return;
    } else if (event.type == SDL_KEYDOWN) {
      if (key == BTN_GP_POWER_ESC) {
        pwrdown = 1;
      } else {
        pwrclicks = 0;
        pwrdown = 0;
      }
      if (gfx_on_key(key, 1) != 0) {
        exit_app = 1;
        return;
      }
    } else if (event.type == SDL_KEYUP) {
      if (key == BTN_GP_POWER_ESC) {
        if (pwrdown) {
          pwrclicks++;
          pwrdown = 0;
          if (pwrclicks >= 3) {
            exit_app = 1;
            return;
          }
        }
      } else {
        pwrclicks = 0;
        pwrdown = 0;
      }
      if (gfx_on_key(key, 0) != 0) {
        exit_app = 1;
        return;
      }
    }
  }
  if (gfx_draw(fps)) {
#ifdef GFX_SDL2
  SDL_RenderPresent(renderer);
#else
  SDL_Flip(screen);
#endif
    gfx_clear();
  }

  busytime = SDL_GetTicks() - start;
  if (delay > busytime) {
    SDL_Delay(delay - busytime);
  }

  float frame_time = (SDL_GetTicks() - start) / 1000.0f;
  if (frame_time > 0) {
    float current_fps = 1.0f / frame_time;
    fps = (0.1f * current_fps) + (0.9f * fps);
  }
}

void gfx_run(void) {
  gfx_yeld = loop;

  gfx_app(1);

  while (!exit_app) {
    loop();
  }
}

void gfx_screenshot(const char * filename) {
#ifdef GFX_SDL2
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

#endif
