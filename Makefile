# Linux build of the core, the app and the stub platform, with their tests.
# The VMS build is build_vms.com.
CC ?= cc
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Werror -g -Isrc/core -Isrc/app -Isrc/plat -Isrc/plat/stub
CORE = src/core/vl_util.c src/core/vl_edit.c src/core/vl_screen.c src/core/vl_ansi.c src/core/vl_lic.c
APP = src/app/vl_app.c src/app/vl_draw.c
HDRS = $(wildcard src/core/*.h src/app/*.h src/plat/*.h src/plat/stub/*.h)

all: test

build/test_core: tests/unit/test_core.c $(CORE) $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/unit/test_core.c $(CORE)

build/test_app: tests/unit/test_app.c $(CORE) $(APP) src/plat/stub/vl_plat_stub.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/unit/test_app.c $(CORE) $(APP) src/plat/stub/vl_plat_stub.c

test: build/test_core build/test_app
	./build/test_core
	./build/test_app

snapshots: build/test_app
	VL_UPDATE_SNAPSHOTS=1 ./build/test_app

clean:
	rm -rf build
.PHONY: all test snapshots clean
