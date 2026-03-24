#include "simplegfx.h"

#if defined(GFX_SDL) || defined(GFX_SDL2)

#ifdef USE_SDL2
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

    if (audio_setup() != 0) {
        printf("Audio setup failed\n");
        return 1;
    }

    return 0;
}

void gfx_cleanup(void) {
#ifdef USE_SDL2
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
#ifdef USE_SDL2
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
#else
    SDL_FillRect(screen, NULL, 0);
#endif
    elm = 0;
}

void gfx_set_color(int r, int g, int b) {
#ifdef USE_SDL2
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
#else
    color = SDL_MapRGB(screen->format, r, g, b);
#endif
}

void gfx_point(int x, int y) {
#ifdef USE_SDL2
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
#ifdef USE_SDL2
    SDL_RenderFillRect(renderer, &rect);
#else
    SDL_FillRect(screen, &rect, color);
#endif
    elm++;
}

void gfx_run(void) {
    uint32_t delay = 1000 / GFX_FPS;
    uint32_t start;
    uint32_t busytime = 0;
    float fps = GFX_FPS;
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
                    return;
                }
            } else if (event.type == SDL_KEYUP) {
                if (gfx_on_key(event.key.keysym.sym, 0) != 0) {
                    return;
                }
            }
        }

        gfx_clear();
        gfx_draw(fps);
#ifdef USE_SDL2
        SDL_RenderPresent(renderer);
#else
        SDL_Flip(screen);
#endif

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
}

void gfx_screenshot(const char * filename) {
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

#endif
