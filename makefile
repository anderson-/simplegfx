LIBS = src
BACKENDS = src/backends
BUILD = build

XTRA_CFLAGS =
ifneq ($(CROSS_COMPILE),)
XTRA_CFLAGS = -I${SYSROOT}/usr/include -L${SYSROOT}/usr/lib
else ifneq ($(shell uname),Darwin)
XTRA_CFLAGS = -I/usr/include -L/usr/lib
endif

INCLUDES = -I$(LIBS) -I$(BACKENDS) $(addprefix -I,$(wildcard $(LIBS)/ext/*))
SOURCES  = $(filter-out $(BACKENDS)/%, $(wildcard $(LIBS)/*.c)) $(wildcard $(LIBS)/ext/*/*.c)
CFLAGS   = -std=c99 -Wall -g -Os -flto ${INCLUDES} ${XTRA_CFLAGS}

EXAMPLES := $(patsubst examples/%.c,%,$(wildcard examples/*.c))
PLATFORMS := sdl sdl1.2

# Platform flags
PLATFORM_FLAGS_sdl = -lSDL2 -DGFX_SDL2 src/backends/gfx_sdl.c
PLATFORM_FLAGS_sdl1.2 = -lSDL -DGFX_SDL src/backends/gfx_sdl.c
PLATFORM_FLAGS_rg35xx = -lSDL -lasound -DGFX_SDL src/backends/gfx_sdl.c

IMAGE = nfriedly/miyoo-toolchain:steward
DOCKER = docker run --platform linux/amd64 --rm -it \
  --user $$(id -u):$$(id -g) \
  -v`pwd`:/src -w/src $(IMAGE)
CONTAINER = container run --platform linux/amd64 --rm -it \
  --uid $$(id -u) --gid $$(id -g) \
  --volume `pwd`:/src \
  --workdir /src $(IMAGE)
define TOOLCHAIN
	@[ "$$(uname)" = "Darwin" ] && command -v container > /dev/null 2>&1 && $(CONTAINER) $(1) || $(DOCKER) $(1)
endef

APP  := example
ADB  := adb
APPS := /mnt/mmc/Roms/APPS

.PHONY: shell
shell:
	nix-shell

.PHONY: all
all: $(foreach p,$(PLATFORMS),$(addprefix $(p)-,$(EXAMPLES)))

# ── sdl / sdl1.2 ─────────────────────────────────────────────────────────────

.PHONY: $(foreach p,$(PLATFORMS),$(addprefix $(p)-,$(EXAMPLES)))
$(foreach p,$(PLATFORMS),$(addprefix $(p)-, $(EXAMPLES))): %:
	$(eval PLAT := $(firstword $(subst -, ,$*)))
	$(eval EXAM := $(patsubst $(PLAT)-%,%,$*))
	mkdir -p ${BUILD}
	${CC} examples/$(EXAM).c ${SOURCES} ${CFLAGS} ${PLATFORM_FLAGS_$(PLAT)} -o ${BUILD}/$*
	chmod +x ${BUILD}/$*

# ── sdl / sdl1.2 screenshot ──────────────────────────────────────────────────

.PHONY: $(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-screenshot))
$(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-screenshot)): %-screenshot:
	$(eval PLAT := $(firstword $(subst -, ,$*)))
	$(eval EXAM := $(patsubst $(PLAT)-%,%,$*))
	mkdir -p ${BUILD}
	${CC} examples/$(EXAM).c ${SOURCES} ${CFLAGS} ${PLATFORM_FLAGS_$(PLAT)} -DSCREENSHOT -o ${BUILD}/$*-screenshot
	chmod +x ${BUILD}/$*-screenshot
	./${BUILD}/$*-screenshot
	convert ${BUILD}/screenshot.bmp ${BUILD}/$*-screenshot.png

# ── run ──────────────────────────────────────────────────────────────────────

.PHONY: $(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-run))
$(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-run)): %-run: %
	./${BUILD}/$*

# ── rg35xx ───────────────────────────────────────────────────────────────────

# Internal cross-compile target (called inside toolchain)
.PHONY: $(addprefix .rg35xx-,$(EXAMPLES))
$(addprefix .rg35xx-,$(EXAMPLES)): .rg35xx-%:
	mkdir -p ${BUILD}
	${CROSS_COMPILE}${CC} examples/$*.c ${SOURCES} ${CFLAGS} ${PLATFORM_FLAGS_rg35xx} -o ${BUILD}/rg35xx-$*.rg35xx
	chmod +x ${BUILD}/rg35xx-$*.rg35xx

# Public cross-compile targets (via toolchain)
.PHONY: $(addprefix rg35xx-,$(EXAMPLES))
$(addprefix rg35xx-,$(EXAMPLES)): rg35xx-%:
	$(call TOOLCHAIN,make .rg35xx-$* CC=gcc)

# Launcher script
.PHONY: $(foreach e,$(EXAMPLES),.rg35xx-$(e)-launcher)
$(foreach e,$(EXAMPLES),.rg35xx-$(e)-launcher): .rg35xx-%-launcher:
	mkdir -p ${BUILD}
	printf '#!/bin/sh\nHOME=$$(dirname "$$0")/$*\ncd $$HOME\nLD_PRELOAD=./j2k.so ./rg35xx-$*.rg35xx\n' > ${BUILD}/rg35xx-$*.sh
	chmod +x ${BUILD}/rg35xx-$*.sh

# Deploy zip
.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-deploy)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-deploy): rg35xx-%-deploy: rg35xx-% .rg35xx-%-launcher
	mkdir -p ${BUILD}/deploy/$*
	cp ${BUILD}/rg35xx-$*.rg35xx ${BUILD}/deploy/$*
	cp ${BUILD}/rg35xx-$*.sh     ${BUILD}/deploy/$*.sh
	cp ${BUILD}/j2k.so           ${BUILD}/deploy/$*
	cd ${BUILD}/deploy && zip -r $*.zip $* $*.sh
	mv ${BUILD}/deploy/$*.zip ${BUILD}/$*.zip
	rm -rf ${BUILD}/deploy

# ADB install
.PHONY: .check-adb
.check-adb:
	@which ${ADB} > /dev/null || (echo "adb not found, please install Android SDK" && exit 1)

.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-install)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-install): rg35xx-%-adb-install: .check-adb rg35xx-% .rg35xx-%-launcher
	${ADB} shell mkdir -p ${APPS}/$*
	${ADB} push ${BUILD}/rg35xx-$*.rg35xx ${APPS}/$*
	${ADB} push ${BUILD}/rg35xx-$*.sh     ${APPS}/$*.sh
	${ADB} push ${BUILD}/j2k.so           ${APPS}/$*
	${ADB} shell chmod 755 ${APPS}/$*/rg35xx-$*.rg35xx
	${ADB} shell chmod 755 ${APPS}/$*.sh

.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-uninstall)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-uninstall): rg35xx-%-adb-uninstall: .check-adb
	${ADB} shell rm -rf ${APPS}/$*
	${ADB} shell rm -f  ${APPS}/$*.sh

.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-kill)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-kill): rg35xx-%-adb-kill: .check-adb
	${ADB} shell kill $$(${ADB} shell ps | grep rg35xx-$*.rg35xx | awk '{print $$2}')

.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-logcat)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-logcat): rg35xx-%-adb-logcat: .check-adb
	${ADB} logcat
