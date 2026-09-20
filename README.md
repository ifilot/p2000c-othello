# Othello for the Philips P2000C

Othello (Reversi) against the computer, for the Philips P2000C running CP/M.
The game uses the terminal board's 512x252 high-resolution graphics mode for
the board and the text plane for the score panel. The user interface is in
Dutch. Three difficulty levels are offered at the start.

![Start screen](docs/start.png)

![Playing field](docs/board.png)

## Play

Copy `OTHELLO.COM` to a CP/M disk and run `OTHELLO`. The start screen (plain
text, so it appears instantly) asks for the difficulty; `1`, `2` or `3`
starts the game.

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
make screenshot       # plain raster dump of the start screen -> build/board.png
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
