# SimpleGFX

SimpleGFX is a straightforward tool designed to help you create graphics using SDL 1.2 or SDL 2. It supports development for the Anbernic RG35xx running Onion OS 1.4.9 and can also be used on macOS for experimenting, building, and testing small graphical applications.

## Features

- Compatibility with SDL 1.2 and SDL 2.
- Supports macOS and RG35xx platforms.
- Basic text rendering functionality.
- 16-bit tone generation for simple sound effects.
- Small terminal helpers for interactive tools and demos.
- Minimal dependencies for portability and ease of use.
- Can be used in embedded projects.

![GFX](build/gfx.png)
![Matrix](build/matrix.png)
![Simpleterm](build/simpleterm.png)
![CLI](build/cli.png)
![Dialogs](build/dialogs.png)

## System Requirements

### macOS

- For native development:
  - GNU Make (check with `make --version`)
- For RG35xx cross-compilation:
  - Docker installed and set up
- To use SDL backend you have the options:
  1. Use nix:
     - `nix-shell` (all dependencies will be set up automatically)
  2. Install SDL2 and SDL 1.2 compatibility:
     - SDL2 (`brew install sdl2`)
     - SDL 1.2 compatibility (`brew install sdl12-compat`)
     - ADB (`brew install android-platform-tools`)

### RG35xx

- Onion OS 1.4.9
- ADB enabled on the device (Consoles -> APPS -> Toggle_ADB)

### Embedded Projects

- Any platform that supports C/C++ and has a display driver.
- Define `WINDOW_WIDTH` and `WINDOW_HEIGHT` constants.
- Use `uint16_t* gfx_get_frame_buffer(void)` to get the frame buffer or implement your own backend.

### Basic Runtime

The runtime is small on purpose. The app draws, reacts to input, and can do a
little work between frames. The backend handles the platform details. SDL and
macOS already provide `main()`, so most examples only implement the callbacks.

#### Writing an app

An app is made of four callbacks:

```c
void gfx_app(int init);               // init == 1 on start, init == 0 on exit
int  gfx_draw(gfx_step_t *s);         // draw a frame; return non-zero to show it
int  gfx_on_key(char key, int down);  // handle one key event; return non-zero to quit
void gfx_process_data(gfx_step_t *s); // background work, runs every frame
```

Use `gfx_app()` for setup and cleanup, `gfx_on_key()` for input, and
`gfx_draw()` for the next visible frame. `gfx_process_data()` runs every tick
after input and drawing; it is the place for small jobs that should not block the
UI, such as terminal input, scripts, or background computation.

See [`examples/gfx.c`](examples/gfx.c) for a graphical example and
[`examples/term.c`](examples/term.c) for a terminal-style app.

#### How the loop works

`gfx_run()` repeats a single tick until something asks it to stop:

1. **Poll** input with `gfx_poll()`. If it returns non-zero, the loop exits.
2. **Draw** by calling `gfx_draw()`. If it returns non-zero, the frame is
   presented with `gfx_flip()` and cleared for the next one.
3. **Work** by calling `gfx_process_data()` for small background tasks.
4. **Sleep** for the rest of the frame to hold `GFX_FPS`.

Each tick fills a shared `gfx_step_t` with timing info, frame count, and the
remaining frame budget. To pause without freezing input, call `gfx_wait(ms)`;
it keeps ticking the loop while it waits.

#### Implementing a backend

A backend connects SimpleGFX to a display and an input source. It provides:

- `int  gfx_setup(void)` / `void gfx_cleanup(void)` - start up and shut down the platform.
- `void gfx_clear(void)`, `gfx_set_color(int r, int g, int b)`, `gfx_point(int x, int y)`,
  `gfx_fill_rect(int x, int y, int w, int h)` - draw into the frame.
- `void gfx_flip(void)` - present the frame to the display.
- `int  gfx_poll(void)` - read input, call `gfx_on_key()` per event, return non-zero to quit.

Timing has portable defaults in [`src/simpletime.h`](src/simpletime.h) (`gfx_time`,
`gfx_sleep`); define `GFX_TIME_OVERRIDE` / `GFX_SLEEP_OVERRIDE` to supply your own.

Audio is optional; implement `gfxa_raw_stream(audio_stream_t fn)` if the backend
should play the generated 16-bit mono stream used by `gfx_beep()` and `gfxa_*`.

For a framebuffer-only target, build with `GFX_BUFFER`: the buffer backend in
[`src/backends/gfx_buffer.c`](src/backends/gfx_buffer.c) implements the drawing
functions over a `uint16_t` framebuffer (get it with `gfx_get_frame_buffer()`).
It ships weak defaults for time, sleep, poll, flip, and audio, so you only fill
in what your hardware actually needs.

Your entry point should call `gfx_setup()`, `gfx_app(1)`, `gfx_run()`,
`gfx_app(0)`, `gfx_cleanup()`. This is exactly what the SDL and macOS backends
do for you.

## Installation

### macOS

Run the following commands to build and run an example:

```bash
# Using native (without nix)
$ make macos-gfx-run

# Using SDL2
$ make sdl-gfx-run

# Using SDL1.2
$ make sdl1.2-gfx-run

# Anbernic RG35xx
$ make rg35xx-gfx-run
```

> `$ make <platform>-<example>-run`

### RG35xx

To build and install the application on RG35xx:

```bash
make rg35xx-gfx-adb-install
```

Ensure that ADB is properly configured and the device is connected before running this command.

## Usage

- Explore the example code in `examples/`.
- `gfx.c` demonstrates drawing, text, input, sound, and timing counters.
- `matrix.c` is a smaller drawing example.
- `term.c` shows the terminal helpers, commands, dialogs, audio commands, and background work running through `gfx_process_data()`.

## License

This project is distributed under the CC0 license, allowing unrestricted use, modification, and distribution.

## Contributions

If you successfully compile this on Windows or Linux and can adapt the Makefile while keeping it organized and consistent, please consider submitting a pull request. Your contributions would be greatly appreciated and integrated into the project.

## Additional Information

- This tool is designed for small, simple projects.
- Includes Makefile targets to simplify building and installing on RG35xx.
- Features are intentionally basic to ensure portability and simplicity.

## Acknowledgments

Special thanks to [https://github.com/nfriedly/miyoo-toolchain](https://github.com/nfriedly/miyoo-toolchain) for providing a Docker image with a precompiled toolchain, streamlining RG35xx development.
