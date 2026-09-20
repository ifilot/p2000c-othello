/* board.c -- Othello rules: setup and the whole-board queries.
 *
 * The board is 64 bytes, row-major, A1 = 0. board_steps gives, per square
 * and direction, how many squares lie before the edge, so the direction
 * walks in rules.asm need no bounds or wrap-around checks.
 */
#include "board.h"

unsigned char board[64];

static const signed char DCOL[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
static const signed char DROW[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

unsigned char board_steps[64][8];

void board_init(void)
{
    unsigned char cell, d, col, row, sc, sr;
    for (cell = 0; cell < 64; cell++) {
        col = cell & 7;
        row = cell >> 3;
        for (d = 0; d < 8; d++) {
            sc = DCOL[d] < 0 ? col : DCOL[d] > 0 ? 7 - col : 7;
            sr = DROW[d] < 0 ? row : DROW[d] > 0 ? 7 - row : 7;
            board_steps[cell][d] = sc < sr ? sc : sr;
        }
    }
    for (cell = 0; cell < 64; cell++)
        board[cell] = EMPTY;
    board[27] = WHITE;  board[28] = BLACK;     /* d4, e4 */
    board[35] = BLACK;  board[36] = WHITE;     /* d5, e5 */
}

unsigned char board_has_move(unsigned char me)
{
    unsigned char cell;
    for (cell = 0; cell < 64; cell++)
        if (board_is_legal(cell, me))
            return 1;
    return 0;
}

unsigned char board_first_move(unsigned char me)
{
    unsigned char cell;
    for (cell = 0; cell < 64; cell++)
        if (board_is_legal(cell, me))
            return cell;
    return 0;
}

unsigned char board_count(unsigned char colour)
{
    unsigned char i, n = 0;
    for (i = 0; i < 64; i++)
        if (board[i] == colour)
            n++;
    return n;
}
