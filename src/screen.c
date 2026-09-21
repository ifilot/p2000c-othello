/* SPDX-License-Identifier: GPL-3.0-only */
/* screen.c -- the board picture.
 *
 * Everything on the board (grid, frame, discs, coordinate glyphs, legal-move
 * hints, the cursor) is composed in the 16 KiB framebuffer. Each cell keeps
 * a record of the appearance last sent, so after a change only the rows of
 * the cells that differ go over the 19200-baud link. A fresh picture is not
 * uploaded whole: the terminal clears its picture RAM on ESC 3 and draws
 * straight lines itself, so the grid goes out as vector commands and only
 * occupied cells, labels and icons as ESC r rows.
 */
#include "video.h"
#include "board.h"
#include "game.h"
#include "screen.h"
#include "sprites.h"

/* Appearance bits: low nibble disc colour, plus hint and cursor flags. */
#define SHOW_COLOUR 0x0F
#define SHOW_HINT   0x10
#define SHOW_CURSOR 0x20
#define SHOW_NONE   0xFF                    /* forces a redraw */

static unsigned char shown[64];             /* appearance last sent per cell */

static unsigned int cell_offset(unsigned char cell)
{
    return (BOARD_TOP + (cell >> 3) * CELL_H) * FB_LINE + LEFT_BYTE + (cell & 7) * CELL_BYTES;
}

static void draw_frame(void)
{
    unsigned char i;
    unsigned int line;

    for (i = 0; i <= 8; i++) {
        line = (BOARD_TOP + i * CELL_H) * FB_LINE;
        video_fill(line + LEFT_BYTE, 0xFF, 8 * CELL_BYTES);        /* x = 8..327 */
        video_or_col(line + LEFT_BYTE + 8 * CELL_BYTES, 0x80, 1);   /* x = 328 */
        video_or_col(BOARD_TOP * FB_LINE + LEFT_BYTE + i * CELL_BYTES, 0x80, BOARD_LINES + 1);
    }
    /* double border: a second line two dots outside the grid, x = 6 and x = 330 */
    for (i = 0; i < 2; i++) {
        line = (i ? BOARD_TOP + BOARD_LINES + FRAME_GAP : BOARD_TOP - FRAME_GAP) * FB_LINE;
        video_fill(line + LEFT_BYTE, 0xFF, 8 * CELL_BYTES);
        video_or_col(line, 0x03, 1);
        video_or_col(line + LEFT_BYTE + 8 * CELL_BYTES, 0xE0, 1);
    }
    line = (BOARD_TOP - FRAME_GAP) * FB_LINE;
    video_or_col(line, 0x02, BOARD_LINES + 2 * FRAME_GAP + 1);
    video_or_col(line + LEFT_BYTE + 8 * CELL_BYTES, 0x20, BOARD_LINES + 2 * FRAME_GAP + 1);
}

static void draw_labels(void)
{
    unsigned char i;
    /* letters centred under the cells, digits centred right of the rows */
    for (i = 0; i < 8; i++) {
        video_blit(glyphs[i], (BOARD_TOP + BOARD_LINES + 8) * FB_LINE + LEFT_BYTE + 2 + i * CELL_BYTES, WH(1, 12));
        video_blit(glyphs[8 + i], (BOARD_TOP + i * CELL_H + 6) * FB_LINE + LEFT_BYTE + 8 * CELL_BYTES + 1, WH(1, 12));
    }
}

/* What the cell should look like right now. */
static unsigned char appearance(unsigned char cell)
{
    unsigned char look = board[cell];
    if (look == EMPTY && !game_over && !demo && to_move == BLACK && board_is_legal(cell, to_move))
        look |= SHOW_HINT;
    if (cell == cursor)
        look |= SHOW_CURSOR;
    return look;
}

/* Recomposes one cell's interior (rows 1..23, keeping the grid line in bit 7). */
static void draw_cell(unsigned char cell)
{
    unsigned int base = cell_offset(cell), offset = base + FB_LINE;
    unsigned char row, look = appearance(cell);
    for (row = 1; row < CELL_H; row++, offset += FB_LINE)
        video_fill(offset, 0, CELL_BYTES);
    video_or_col(base + FB_LINE, 0x80, CELL_H - 1);
    if (look & SHOW_COLOUR)
        video_blit((look & SHOW_COLOUR) == BLACK ? sprite_black : sprite_white,
                   base + 3 * FB_LINE, WH(CELL_BYTES, 18));
    if (look & SHOW_HINT)
        video_blit(sprite_hint, base + 11 * FB_LINE + 2, WH(1, 3));
    if (look & SHOW_CURSOR)
        video_blit(sprite_cursor, base + 1 * FB_LINE, WH(CELL_BYTES, 22));
    shown[cell] = look;
}

/* Sends rows [first, first + rows) of one cell, one short ESC r write per row. */
static void flush_rows(unsigned char cell, unsigned char first, unsigned char rows)
{
    video_flush_rect(COLROW(LEFT_BYTE + (cell & 7) * CELL_BYTES,
                            BOARD_TOP + (cell >> 3) * CELL_H + first),
                     WH(CELL_BYTES, rows));
}

/* Redraws and re-sends every cell whose appearance changed. Only the rows
 * that can differ are sent: the hint mark lives in rows 11-13, the cursor
 * brackets in rows 1-4 and 19-22, a disc change needs the whole interior. */
void sync_cells(void)
{
    unsigned char cell, diff;
    for (cell = 0; cell < 64; cell++) {
        diff = appearance(cell) ^ shown[cell];
        if (!diff)
            continue;
        draw_cell(cell);
        if (diff & ~(SHOW_HINT | SHOW_CURSOR)) {
            flush_rows(cell, 1, CELL_H - 1);
            continue;
        }
        if (diff & SHOW_CURSOR)
            flush_rows(cell, 1, 4);
        if (diff & SHOW_HINT)
            flush_rows(cell, 11, 3);
        if (diff & SHOW_CURSOR)
            flush_rows(cell, 19, 4);
    }
}

/* Draws a straight line with the terminal's vector commands (ESC m / ESC M),
 * in framebuffer coordinates (dot x, line from the top). Axis-aligned lines
 * come out identical to the framebuffer's, so later cell uploads match. */
static void vector(unsigned int x0, unsigned int line0, unsigned int x1, unsigned int line1)
{
    conout(27); conout('m'); conout(x0 & 0xFF); conout(x0 >> 8); conout(251 - line0);
    conout(27); conout('M'); conout(x1 & 0xFF); conout(x1 >> 8); conout(251 - line1);
}

/* Sends the composed frame the fast way: the terminal clears its picture RAM
 * on ESC 3, so only the grid (as 22 vectors, ~220 bytes) and the occupied
 * cells, labels and icons (~3 KB of row uploads) need to go over the link,
 * instead of the full 16 KiB. Roughly 1.7 s at 19200 baud instead of 8.5 s. */
void flush_frame(void)
{
    unsigned char i, cell;
    unsigned int x;
    for (i = 0; i <= 8; i++) {
        vector(8, BOARD_TOP + i * CELL_H, 328, BOARD_TOP + i * CELL_H);
        x = 8 + i * 40;
        vector(x, BOARD_TOP, x, BOARD_TOP + BOARD_LINES);
    }
    vector(6, BOARD_TOP - FRAME_GAP, 330, BOARD_TOP - FRAME_GAP);
    vector(6, BOARD_TOP + BOARD_LINES + FRAME_GAP, 330, BOARD_TOP + BOARD_LINES + FRAME_GAP);
    vector(6, BOARD_TOP - FRAME_GAP, 6, BOARD_TOP + BOARD_LINES + FRAME_GAP);
    vector(330, BOARD_TOP - FRAME_GAP, 330, BOARD_TOP + BOARD_LINES + FRAME_GAP);
    for (cell = 0; cell < 64; cell++)
        if (shown[cell] != 0)
            flush_rows(cell, 1, CELL_H - 1);
    for (i = 0; i < 8; i++) {
        video_flush_rect(COLROW(LEFT_BYTE + 2 + i * CELL_BYTES, BOARD_TOP + BOARD_LINES + 8), WH(1, 12));
        video_flush_rect(COLROW(LEFT_BYTE + 8 * CELL_BYTES + 1, BOARD_TOP + i * CELL_H + 6), WH(1, 12));
    }
    video_flush_rect(COLROW(PANEL_ICON_BYTE, 5 * TEXT_ROW_LINES + 1), WH(2, 10));
    video_flush_rect(COLROW(PANEL_ICON_BYTE, 6 * TEXT_ROW_LINES + 1), WH(2, 10));
}

void draw_board(void)
{
    unsigned char cell;
    for (cell = 0; cell < 64; cell++)
        shown[cell] = SHOW_NONE;
    video_clear();
    draw_frame();
    draw_labels();
    for (cell = 0; cell < 64; cell++)
        draw_cell(cell);
    /* score icons beside the text panel rows 5 and 6 (12 dots per text row) */
    video_blit(icon_black, (5 * TEXT_ROW_LINES + 1) * FB_LINE + PANEL_ICON_BYTE, WH(2, 10));
    video_blit(icon_white, (6 * TEXT_ROW_LINES + 1) * FB_LINE + PANEL_ICON_BYTE, WH(2, 10));
}

/* Uploads only the lit parts of the framebuffer: one ESC r per run of
 * non-zero bytes in a line (short gaps are bridged, a header costs 7 bytes). */
void flush_sparse(void)
{
    unsigned char line, first, last, x;
    const unsigned char *row;
    for (line = 0; line < FB_LINES; line++) {
        row = framebuffer + line * FB_LINE;
        x = 0;
        while (x < FB_LINE) {
            while (x < FB_LINE && row[x] == 0)
                x++;
            if (x == FB_LINE)
                break;
            first = last = x;
            while (x < FB_LINE) {                /* extend over gaps shorter than a header */
                if (row[x] != 0)
                    last = x;
                else if (x - last >= 7)
                    break;
                x++;
            }
            video_flush_rect(COLROW(first, line), WH(last - first + 1, 1));
        }
    }
}

