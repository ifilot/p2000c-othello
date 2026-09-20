# Othello for the Philips P2000C

[![Build](https://github.com/ifilot/p2000c-othello/actions/workflows/build.yml/badge.svg)](https://github.com/ifilot/p2000c-othello/actions/workflows/build.yml)
[![Version](https://img.shields.io/badge/version-1.0.0-blue)](https://github.com/ifilot/p2000c-othello/releases)
[![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

Othello (Reversi) against the computer, for the Philips P2000C running CP/M.
The game uses the terminal board's 512x252 high-resolution graphics mode for
the board and the text plane for the score panel. The user interface is in
Dutch. Three difficulty levels are offered at the start.

<p align="center">
  <img src="docs/start.png" alt="Start screen" width="48%">
  <img src="docs/board.png" alt="Playing field" width="48%">
</p>

## Play

Download `OTHELLO.COM` from the [releases](https://github.com/ifilot/p2000c-othello/releases)
(or the latest [build artifact](https://github.com/ifilot/p2000c-othello/actions)),
copy it to a CP/M disk and run `OTHELLO`. The start screen (plain text, so it
appears instantly) asks for the difficulty; `1`, `2` or `3` starts the game.

| Key | Action |
| --- | --- |
| Cursor keys or `W` `A` `S` `D` | Move the cursor |
| `RETURN` or space | Place a disc |
| `H` | Help screen with the rules (plain text mode) |
| `N` | Back to the start screen for a new game |
| `Q` | Quit, after confirmation (immediate on the start screen) |

You play Black and move first; the P2000C plays White. Legal moves are marked
with a dot. Levels: **1** plays the move that flips the most discs (corners
preferred), **2** and **3** search two and three plies ahead with a positional
evaluation. Level 3 thinks up to about three seconds per move.

## Build

The program is C (Z88DK/sdcc) with the display driver and the rules' inner
loops in Z80 assembly. It is compiled with the `z88dk/z88dk` Docker image:

```sh
make build            # -> build/OTHELLO.COM
```

The other targets use the sibling checkouts
[p2000c-cpm-disk-tool](https://github.com/ifilot/p2000c-cpm-disk-tool)
(headless emulator, CP/M disk images) and
[p2000c-emulator](https://github.com/ifilot/p2000c-emulator) (graphical
emulator, character-ROM font):

```sh
make run              # open the game in the graphical emulator (WSLg/Linux)
make screenshot       # plain raster dump (green on black) -> build/board.png
make test             # full games against the computer in the headless emulator
make sprites          # regenerate src/sprites.h from the font sheet
python3 tools/bench.py 3   # emulated seconds per round at a level
```

## Layout

| File | Contents |
| --- | --- |
| `src/main.c` | Screen composition, text panel, input and game flow |
| `src/board.c`, `src/board.h` | Board setup and whole-board queries |
| `src/rules.asm` | Direction walks (flip count, legality, playing a move) and the positional evaluation |
| `src/cpu.c` | Computer player: greedy level and alpha-beta search |
| `src/video.asm`, `src/video.h` | Framebuffer primitives, `ESC r` bulk writes, BIOS console I/O |
| `src/sprites.h` | Generated disc, cursor and coordinate-glyph bitmaps |
| `tools/` | Sprite generator, emulator launchers, screenshot, tests, benchmark |

## Display notes

The terminal draws graphics at the text raster's dot pitch; on the 4:3 CRT a
dot is 3:5, so the 40x24-dot cells are square and the 30x18-dot discs round.
The whole 16 KiB frame is composed in RAM and sent once with an `ESC r` bulk
write (about four seconds over the serial link); afterwards only the rows of
the cells that changed are re-sent. Raw bytes go through BIOS `CONOUT`,
because BDOS console output filters control characters.

## Continuous integration

The [workflow](.github/workflows/build.yml) builds `OTHELLO.COM` with the
Z88DK Docker image on every push and pull request and uploads it as an
artifact; pushing a `v*` tag publishes a GitHub release with the binary and a
ZIP that includes the license and this README.

## License

GNU General Public License v3.0; see [LICENSE](LICENSE). The character-ROM
font sheet used only by the screenshot tooling belongs to the
[p2000c-emulator](https://github.com/ifilot/p2000c-emulator) project.
