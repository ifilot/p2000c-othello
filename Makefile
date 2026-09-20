# Othello for the Philips P2000C: C (Z88DK/sdcc) + Z80 assembly, CP/M target.
#
# The compiler runs in the z88dk/z88dk Docker image. Screenshots, tests and
# `make run` need the sibling p2000c-cpm-disk-tool checkout (headless emulator
# and dist/pro/ disk images) and, for the character-ROM font, p2000c-emulator.

VERSION    = 1.0.0
BUILD_DATE = $(shell date +%Y-%m-%d)

ZCC      = docker run --rm --user $(shell id -u):$(shell id -g) -v "$(CURDIR)":/src -w /src z88dk/z88dk zcc
ZCCFLAGS = +cpm -vn -clib=sdcc_iy -O3 -SO3 --opt-code-speed --max-allocs-per-node200000 \
           -Ibuild -create-app

SOURCES = src/main.c src/board.c src/cpu.c src/video.asm src/rules.asm
HEADERS = src/video.h src/board.h src/cpu.h src/sprites.h src/version.h
COM     = build/OTHELLO.COM

.PHONY: all build run screenshot test sprites clean

all: build

build: $(COM)

$(COM): $(SOURCES) $(HEADERS) Makefile
	mkdir -p build
	printf '#define VERSION "%s"\n#define BUILD_DATE "%s"\n' "$(VERSION)" "$(BUILD_DATE)" > build/build_info.h
	$(ZCC) $(ZCCFLAGS) $(SOURCES) -o build/othello
	rm -f build/othello build/othello_CODE.bin

# Regenerate disc/glyph bitmaps (needs the p2000c-emulator font sheet).
sprites:
	python3 tools/gen_sprites.py

# Open the game in the graphical emulator.
run: build
	python3 tools/run.py

# Plain raster screenshot of the start screen -> build/board.png
# (add --wait-for/actions via tools/render.py for other moments)
screenshot: build
	python3 tools/render.py

# Full games against the computer in the headless emulator.
test: build
	python3 tools/test_cpu_game.py

clean:
	rm -rf build
