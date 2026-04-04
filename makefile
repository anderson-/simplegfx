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
TEST     = test/microcuts/src/microcuts.c -Itest/microcuts/src/
CFLAGS   = -std=c99 -Wall -g -Os -flto ${INCLUDES} ${XTRA_CFLAGS}

EXAMPLES := $(patsubst examples/%.c,%,$(wildcard examples/*.c))
PLATFORMS := sdl sdl1.2 macos

# Platform flags
PLATFORM_FLAGS_sdl = -lSDL2 -DGFX_SDL2 src/backends/gfx_sdl.c
PLATFORM_FLAGS_sdl1.2 = -lSDL -DGFX_SDL src/backends/gfx_sdl.c
PLATFORM_FLAGS_rg35xx = -lSDL -lasound -DGFX_SDL src/backends/gfx_sdl.c
PLATFORM_FLAGS_macos = -framework Cocoa -framework AudioToolbox -lobjc -DGFX_MACOS src/backends/gfx_macos.m

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

# ── desktop builds ───────────────────────────────────────────────────────────

.PHONY: $(foreach p,$(PLATFORMS),$(addprefix $(p)-,$(EXAMPLES)))
$(foreach p,$(PLATFORMS),$(addprefix $(p)-, $(EXAMPLES))): %:
	$(eval PLAT := $(firstword $(subst -, ,$*)))
	$(eval EXAM := $(patsubst $(PLAT)-%,%,$*))
	mkdir -p ${BUILD}
	${CC} examples/$(EXAM).c ${SOURCES} ${CFLAGS} ${PLATFORM_FLAGS_$(PLAT)} -o ${BUILD}/$*
	chmod +x ${BUILD}/$*

# ── debug builds ─────────────────────────────────────────────────────────────

.PHONY: $(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-debug))
$(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-debug)): %-debug:
	$(eval PLAT := $(firstword $(subst -, ,$*)))
	$(eval EXAM := $(patsubst $(PLAT)-%,%,$*))
	mkdir -p ${BUILD}
	${CC} examples/$(EXAM).c ${SOURCES} ${CFLAGS} ${PLATFORM_FLAGS_$(PLAT)} -DDEBUG -o ${BUILD}/$*-debug
	chmod +x ${BUILD}/$*-debug
	./${BUILD}/$*-debug

# ── screenshot builds ────────────────────────────────────────────────────────

.PHONY: $(foreach p,sdl sdl1.2,$(foreach e,$(EXAMPLES),$(p)-$(e)-screenshot))
$(foreach p,sdl sdl1.2,$(foreach e,$(EXAMPLES),$(p)-$(e)-screenshot)): %-screenshot:
	$(eval PLAT := $(firstword $(subst -, ,$*)))
	$(eval EXAM := $(patsubst $(PLAT)-%,%,$*))
	mkdir -p ${BUILD}
	${CC} examples/$(EXAM).c ${SOURCES} ${CFLAGS} ${PLATFORM_FLAGS_$(PLAT)} -DSCREENSHOT -o ${BUILD}/$*-screenshot
	chmod +x ${BUILD}/$*-screenshot
	./${BUILD}/$*-screenshot
	magick ${BUILD}/screenshot*.bmp ${BUILD}/$*-screenshot.png

# ── run ──────────────────────────────────────────────────────────────────────

.PHONY: $(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-run))
$(foreach p,$(PLATFORMS),$(foreach e,$(EXAMPLES),$(p)-$(e)-run)): %-run: %
	./${BUILD}/$*

# ── test ─────────────────────────────────────────────────────────────────────

.PHONY: test
test:
	${CC} test/test.c ${SOURCES} ${TEST} ${CFLAGS} -DTEST -o ${BUILD}/test
	chmod +x ${BUILD}/test
	./${BUILD}/test

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
	printf '#!/bin/sh\ncd ${APPS}/$*\nSDL_NOMOUSE=1 LD_PRELOAD=./j2k.so ./rg35xx-$*.rg35xx\n' > ${BUILD}/rg35xx-$*.sh
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

.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-run)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-run): rg35xx-%-run: rg35xx-%-adb-install
	MAIN_PID=$$($(ADB) shell ps | awk '/\.\/main/{print $$2}'); \
	LOOP_PID=$$($(ADB) shell ps | awk '/\.\/main/{print $$3}'); \
	GRAND_PID=$$($(ADB) shell ps | awk -v p=$$LOOP_PID '$$2==p{print $$3}'); \
	$(ADB) shell " \
		trap 'kill -CONT $$GRAND_PID 2>/dev/null; kill -CONT $$LOOP_PID 2>/dev/null; exit' INT TERM HUP EXIT; \
		kill -STOP $$GRAND_PID && kill -STOP $$LOOP_PID && \
		kill $$MAIN_PID && \
		busybox chroot /cfw /bin/sh -c \"${APPS}/$*.sh\"; \
		kill -CONT $$GRAND_PID 2>/dev/null; kill -CONT $$LOOP_PID 2>/dev/null \
	"

.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-uninstall)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-uninstall): rg35xx-%-adb-uninstall: .check-adb
	${ADB} shell rm -rf ${APPS}/$*
	${ADB} shell rm -f  ${APPS}/$*.sh

.PHONY: $(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-kill)
$(foreach e,$(EXAMPLES),rg35xx-$(e)-adb-kill): rg35xx-%-adb-kill: .check-adb
	${ADB} shell kill $$(${ADB} shell ps | grep rg35xx-$*.rg35xx | awk '{print $$2}')

.PHONY: rg35xx-adb-logcat
rg35xx-adb-logcat: .check-adb
	${ADB} logcat

.PHONY: rg35xx-adb-shell
rg35xx-adb-shell: .check-adb
	${ADB} shell
