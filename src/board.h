/* SPDX-License-Identifier: GPL-3.0-only */
/* board.h -- Othello rules on a 64-byte board (row-major, A1 = 0).
 *
 * The direction walks live in rules.asm; the macros pack (cell, colour)
 * into the single fastcall word those routines take. Arguments are
 * evaluated more than once, so pass plain variables. */
#ifndef BOARD_H
#define BOARD_H

#define EMPTY 0
#define BLACK 1
#define WHITE 2

extern unsigned char board[64];
extern unsigned char board_steps[64][8];    /* squares available per direction */

extern unsigned char board_flips_fc(unsigned int cell_me) __z88dk_fastcall;
extern unsigned char board_legal_fc(unsigned int cell_me) __z88dk_fastcall;
extern void board_play_fc(unsigned int cell_me) __z88dk_fastcall;
extern int board_evaluate(unsigned int me) __z88dk_fastcall;

#define CELL_ME(cell, me)        ((unsigned int)(cell) | ((unsigned int)(me) << 8))
/* Opponent discs a move would flip in total, 0 if the move is not legal. */
#define board_flips(cell, me)    board_flips_fc(CELL_ME(cell, me))
#define board_is_legal(cell, me) board_legal_fc(CELL_ME(cell, me))
/* Places a disc and flips everything it brackets; the caller checks legality. */
#define board_play(cell, me)     board_play_fc(CELL_ME(cell, me))

/* Standard four-disc opening position (also builds board_steps). */
extern void board_init(void);
extern unsigned char board_has_move(unsigned char me);
extern unsigned char board_first_move(unsigned char me);   /* row-major order */
extern unsigned char board_count(unsigned char colour);

#endif
