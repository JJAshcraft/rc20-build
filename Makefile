# Bramble build
#   make            desktop build (needs libsdl2-dev)  -> build/bramble
#   make test       headless engine test               -> build/out/*.wav
#   make r36        R36S / aarch64 build               -> build/r36/bramble.aarch64
#   make package    Ports folder + zip                 -> dist/Bramble.zip
#
# The aarch64 build links against a stub libSDL2 made from the symbols the
# program uses, so it runs against the SDL2 the handheld's OS already ships.

SRC_CORE := src/params.c src/wavetable.c src/synth.c src/preset.c
SRC_APP  := $(SRC_CORE) src/font.c src/ui.c src/main.c
CFLAGS   ?= -O2
CFLAGS   += -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-unused-parameter
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null)

CROSS    ?= aarch64-linux-gnu-
XCC      := $(CROSS)gcc
XCFLAGS  := -O2 -mcpu=cortex-a35 -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wno-unused-parameter
SDL_INC  ?= /usr/include/SDL2
R36      := build/r36

.PHONY: all test r36 package clean

all: build/bramble

build/bramble: $(SRC_APP) src/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $(SRC_APP) $(SDL_LIBS) -lm

build/render: tools/render.c $(SRC_CORE) src/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tools/render.c $(SRC_CORE) -lm

test: build/render
	@mkdir -p build/out
	./build/render build/out 3

# --- aarch64 ---
$(R36)/include/SDL2/_real_SDL_config.h:
	@mkdir -p $(R36)/include/SDL2
	f=$$(find /usr/include -path '*SDL2/_real_SDL_config.h' | head -1); \
	  grep -v -E 'IMMINTRIN|SSE|MMX|3DNOW|AVX|ALTIVEC|LSX|LASX' $${f:-/dev/null} > $@ || true

$(R36)/objs.stamp: $(SRC_APP) src/*.h $(R36)/include/SDL2/_real_SDL_config.h
	@mkdir -p $(R36)/obj
	for f in $(SRC_APP); do \
	  $(XCC) $(XCFLAGS) -I$(R36)/include -I$(SDL_INC) -D_REENTRANT -DSDL_DISABLE_IMMINTRIN_H -include src/glibc_compat.h -c $$f -o $(R36)/obj/$$(basename $$f .c).o || exit 1; \
	done
	touch $@

$(R36)/stub/libSDL2.so: $(R36)/objs.stamp
	@mkdir -p $(R36)/stub
	$(CROSS)nm -u $(R36)/obj/*.o | awk '{print $$2}' | grep '^SDL_' | sort -u | \
	  sed 's/.*/void &(void) {}/' > $(R36)/stub/sdl_stub.c
	$(XCC) -shared -fPIC -Wl,-soname,libSDL2-2.0.so.0 -o $@ $(R36)/stub/sdl_stub.c

$(R36)/bramble.aarch64: $(R36)/objs.stamp $(R36)/stub/libSDL2.so
	$(XCC) -o $@ $(R36)/obj/*.o -L$(R36)/stub -lSDL2 -lm -Wl,--as-needed
	$(CROSS)strip $@

r36: $(R36)/bramble.aarch64
	@echo "glibc symbols needed:"; $(CROSS)objdump -T $< | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -3

package: r36
	rm -rf dist && mkdir -p dist/ports/bramble
	cp port/Bramble.sh dist/ports/
	cp $(R36)/bramble.aarch64 port/bramble/bramble.cfg port/bramble/README.txt dist/ports/bramble/
	chmod +x dist/ports/Bramble.sh dist/ports/bramble/bramble.aarch64
	cd dist && zip -qr Bramble.zip ports && echo "dist/Bramble.zip"

clean:
	rm -rf build dist
