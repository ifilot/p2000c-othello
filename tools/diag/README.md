# Terminal diagnostics for real hardware

Development tool, not part of the game or its deployment image. `DIAG.COM`
isolates the assumptions the game makes about the P2000C terminal board and
was used to establish the `ESC r` rules below on a real machine. Copy it to a CP/M disk, run `DIAG`, and photograph the screen after
each test. Every test is announced on a cleared text screen: press a key to
run it, then a key to continue; each banner states the expected picture.

| Test | What it exercises | Result on hardware |
| --- | --- | --- |
| T1 | 24 rows of 79 characters, BIOS `CONOUT` at full speed | passes |
| T2 | the same rows through BDOS function 6 | passes |
| T3 | the same rows through BIOS `CONOUT` with ~2 ms per byte | passes |
| T4 | `ESC 3`, a box drawn with `ESC m`/`ESC M`, a dot, a text label | passes |
| T5 | three `ESC r` uploads: 64 bytes at (0,251), 64 at (0,0), 640 at (0,200) | passes |
| T6 | `ESC r` of two lines at the bottom line: solid, then dotted; the dotted line must appear *above* the solid one | to be confirmed |
| T7 | `ESC r` of an 11-line staircase from 11 lines above the bottom: the steps must climb up and to the right | to be confirmed |

Rebuild with `z80asm -o DIAG.COM diag.asm`.

## Findings

Two rules for the picture upload were established on the real terminal:

1. **A count with a zero low byte fails** (1000 bytes work, 1024 and 16128
   do not); the data that follows is then processed as text.
2. **The picture RAM runs bottom-up.** The bytes of an upload fill the given
   line left to right and then continue on the line *above* it, wrapping
   from the top line to the bottom one. A multi-line upload must therefore
   start at the lowest line of its region and send the lines from bottom to
   top. Uploading top-down lines produced the photographed artifacts
   (hourglass discs, shuffled glyph rows); simulating this rule on the
   framebuffer reproduced both photographs exactly. Single-line uploads, as
   CHESS.COM uses for its whole board, are unaffected.

Framebuffer bytes include control codes (`0x18` resets the terminal), so any
bytes that spill into text leave the terminal confused for later screens.
The game uploads the frame in 15-line pieces, each sent bottom-up from its
lowest line, and the headless emulator models both rules.
