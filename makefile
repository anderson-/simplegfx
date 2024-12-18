
DOCKER = docker run --platform linux/amd64 --rm -it --user $$(id -u):$$(id -g) -v`pwd`:/src -w/src

LIBS = src
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

INCLUDES     = $(addprefix -I, $(LIBS))
SOURCES      = $(foreach dir, $(LIBS), $(wildcard $(dir)/*.c))
SRC_FILES    = $(foreach dir, $(LIBS), $(wildcard $(dir)/*.c)) $(foreach dir, $(LIBS), $(wildcard $(dir)/*.h))
CFLAGS       = -std=c99 -Wall -g -Os -flto ${INCLUDES} ${SOURCES} ${XTRA_CFLAGS}
OUTPUT       = -o ${BUILD}/gfx

.PHONY: gfx
gfx:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${CFLAGS} -lSDL2 -DUSE_SDL2 ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: gfx1.2
gfx1.2:
	mkdir -p ${BUILD}
	${CC} ${MAIN_SRC} ${CFLAGS} -lSDL -ldl -lm ${OUTPUT}
	chmod +x ${BUILD}/gfx

.PHONY: .RG35xx
.RG35xx:
	mkdir -p ${BUILD}
	${CROSS_COMPILE}${CC} ${MAIN_SRC} ${CFLAGS} -lSDL -lasound ${OUTPUT}.rg35xx
	chmod +x ${BUILD}/gfx.rg35xx

.PHONY: RG35xx
RG35xx:
	${DOCKER} nfriedly/miyoo-toolchain:steward make .RG35xx CC=gcc

APP := example
ADB := adb
APPS := /mnt/mmc/Roms/APPS

.PHONY: .RG35xx-sh
.RG35xx-sh:
	echo "#!/bin/sh" > ${BUILD}/launcher.sh
	echo "HOME=${APPS}/${APP}" >> ${BUILD}/launcher.sh
	echo "cd \$$HOME" >> ${BUILD}/launcher.sh
	echo "LD_PRELOAD=./j2k.so ./gfx.rg35xx" >> ${BUILD}/launcher.sh
	chmod +x ${BUILD}/launcher.sh

.PHONY: RG35xx-deploy
RG35xx-deploy: RG35xx .RG35xx-sh
	rm -rf ${BUILD}/deploy
	mkdir -p ${BUILD}/deploy/${APP}
	cp ${BUILD}/gfx.rg35xx ${BUILD}/deploy/${APP}
	cp ${BUILD}/launcher.sh ${BUILD}/deploy/${APP}.sh
	cp ${BUILD}/j2k.so ${BUILD}/deploy/${APP}
	cd ${BUILD}/deploy && zip -r ${APP}.zip ${APP} ${APP}.sh
	mv ${BUILD}/deploy/${APP}.zip ${BUILD}/${APP}.zip

.PHONY: .check-adb
.check-adb:
	@which ${ADB} > /dev/null || (echo "adb not found, please install Android SDK" && exit 1)

.PHONY: RG35xx-adb-install
RG35xx-adb-install: .check-adb RG35xx .RG35xx-sh
	${ADB} shell mkdir -p ${APPS}/${APP}
	${ADB} push ${BUILD}/gfx.rg35xx ${APPS}/${APP}
	${ADB} push ${BUILD}/launcher.sh ${APPS}/${APP}.sh
	${ADB} push ${BUILD}/j2k.so ${APPS}/${APP}
	${ADB} shell chmod 755 ${APPS}/${APP}/gfx.rg35xx
	${ADB} shell chmod 755 ${APPS}/${APP}.sh

.PHONY: RG35xx-adb-uninstall
RG35xx-adb-uninstall: .check-adb
	${ADB} shell rm -rf ${APPS}/${APP}
	${ADB} shell rm -f ${APPS}/${APP}.sh

.PHONY: RG35xx-adb-kill
RG35xx-adb-kill: .check-adb
	${ADB} shell kill $$(${ADB} shell ps | grep gfx.rg35xx | awk '{print $$2}')

.PHONY: RG35xx-adb-logcat
RG35xx-adb-logcat: .check-adb
	${ADB} logcat