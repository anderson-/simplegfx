
DOCKER = docker run --platform linux/amd64 --rm -it --user $$(id -u):$$(id -g) -v`pwd`:/src -w/src

LIBS = src
BACKENDS = src/backends
MAIN_SRC = examples/gfx.c
BUILD = build

ifeq ($(CROSS_COMPILE),)
ifeq ($(shell uname),Darwin)
XTRA_CFLAGS = -I/opt/homebrew/include -L/opt/homebrew/lib
else
XTRA_CFLAGS = -I/usr/include -L/usr/lib
endif
else
XTRA_CFLAGS = -I${SYSROOT}/usr/include -L${SYSROOT}/usr/lib
endif

INCLUDES     = -I$(LIBS) -I$(BACKENDS)
# Generic sources (excluding backends)
SOURCES      = $(filter-out $(BACKENDS)/%, $(wildcard $(LIBS)/*.c))
CFLAGS       = -std=c99 -Wall -g -Os -flto ${INCLUDES} ${XTRA_CFLAGS}
OUTPUT       = -o ${BUILD}/gfx

.PHONY: all
all: gfx

.PHONY: gfx
gfx:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${SOURCES} src/backends/gfx_sdl.c ${CFLAGS} -lSDL2 -DGFX_SDL2 -DUSE_SDL2 ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: gfx1.2
gfx1.2:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${SOURCES} src/backends/gfx_sdl.c ${CFLAGS} -lSDL -DGFX_SDL ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: buffer
buffer:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${SOURCES} src/backends/gfx_buffer.c ${CFLAGS} -DGFX_BUFFER ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: basic
basic:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${SOURCES} src/backends/gfx_basic.c ${CFLAGS} -DGFX_BASIC ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: .RG35xx
.RG35xx:
	mkdir -p ${BUILD}
	${CROSS_COMPILE}${CC} ${MAIN_SRC} ${SOURCES} src/backends/gfx_sdl.c ${CFLAGS} -lSDL -lasound -DGFX_SDL ${OUTPUT}.rg35xx
	chmod +x ${BUILD}/gfx.rg35xx

.PHONY: RG35xx
RG35xx:
	${DOCKER} nfriedly/miyoo-toolchain:steward make .RG35xx CC=gcc

.PHONY: screenshot
screenshot:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${SOURCES} src/backends/gfx_sdl.c ${CFLAGS} -lSDL2 -DGFX_SDL2 -DUSE_SDL2 -DSCREENSHOT ${OUTPUT}
	chmod +x ${BUILD}/gfx
	./${BUILD}/gfx
	convert ${BUILD}/screenshot.bmp ${BUILD}/screenshot.png

.PHONY: screenshot1.2
screenshot1.2:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${SOURCES} src/backends/gfx_sdl.c ${CFLAGS} -lSDL -DGFX_SDL -DSCREENSHOT ${OUTPUT}
	chmod +x ${BUILD}/gfx
	./${BUILD}/gfx
	convert ${BUILD}/screenshot.bmp ${BUILD}/screenshot.png

.PHONY: term
term:
	mkdir -p ${BUILD}
	${CC} examples/term.c ${SOURCES} src/backends/gfx_sdl.c ${CFLAGS} -lSDL2 -DGFX_SDL2 -DUSE_SDL2 ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: term1.2
term1.2:
	mkdir -p ${BUILD}
	${CC} examples/term.c ${SOURCES} src/backends/gfx_sdl.c ${CFLAGS} -lSDL -DGFX_SDL ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: shell
shell:
	nix-shell