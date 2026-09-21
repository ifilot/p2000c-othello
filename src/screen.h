/* SPDX-License-Identifier: GPL-3.0-only */
/* screen.h -- the board picture: composition in the framebuffer and uploads.
 *
 * Geometry in dots; the board's left edge sits on byte 1 (x = 8). Dots have
 * a 3:5 pitch on the CRT, so a 40x24-dot cell is square. */
#ifndef SCREEN_H
#define SCREEN_H

#define BOARD_TOP   12
#define CELL_H      24
#define CELL_BYTES  5
#define LEFT_BYTE   1
#define BOARD_LINES (8 * CELL_H)
#define FRAME_GAP   2                       /* outer frame two dots outside the grid */
#define PANEL_ICON_BYTE 45                  /* score icons, left of the text panel */
#define TEXT_ROW_LINES  12                  /* dots per text-plane row */

/* Composes the whole picture (grid, labels, every cell, icons) in RAM. */
extern void draw_board(void);

/* Redraws and re-sends every cell whose appearance changed. */
extern void sync_cells(void);

/* Sends the composed picture after ESC 3: grid as vectors, the rest as row uploads. */
extern void flush_frame(void);

/* Sends only the lit runs of every framebuffer line (title picture). */
extern void flush_sparse(void);

#endif
