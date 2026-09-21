/* SPDX-License-Identifier: GPL-3.0-only */
/* cpu.c -- computer player.
 *
 * Level 1 is the P2000M Othello heuristic: every legal move is scored by
 * its immediate captures plus a ten-point bonus for a corner, which can
 * never be recaptured. Levels 2 and 3 run a negamax search with alpha-beta
 * pruning to that many plies, judging leaves by the positional weight table
 * in rules.asm (corners good, the squares next to them dangerous) and
 * finished games by the disc difference. Squares are visited in a fixed
 * order and replaced only on a strictly better score, so every level is
 * deterministic and complete emulator games are reproducible.
 */
#include <string.h>
#include "board.h"
#include "cpu.h"

unsigned char cpu_level = 1;

/* Squares by descending weight: trying strong moves first makes alpha-beta
 * prune much more, and the fixed order keeps the choice deterministic. */
static const unsigned char ORDER[64] = {
     0,  7, 56, 63,  2,  5, 16, 23, 40, 47, 58, 61,  3,  4, 24, 31,
    32, 39, 59, 60, 18, 19, 20, 21, 26, 27, 28, 29, 34, 35, 36, 37,
    42, 43, 44, 45, 10, 11, 12, 13, 17, 22, 25, 30, 33, 38, 41, 46,
    50, 51, 52, 53,  1,  6,  8, 15, 48, 55, 57, 62,  9, 14, 49, 54,
};

static unsigned char is_corner(unsigned char cell)
{
    unsigned char col = cell & 7, row = cell >> 3;
    return (col == 0 || col == 7) && (row == 0 || row == 7);
}

static unsigned char greedy(unsigned char me, unsigned char *cell)
{
    unsigned char c, flips, score, found = 0, best = 0;
    for (c = 0; c < 64; c++) {
        flips = board_flips(c, me);
        if (!flips)
            continue;
        score = flips + (is_corner(c) ? 10 : 0);
        if (!found || score > best) {
            found = 1;
            best = score;
            *cell = c;
        }
    }
    return found;
}

/* Finished game: the disc difference dominates every positional score. */
static int final_score(unsigned char me)
{
    return ((int)board_count(me) - (int)board_count(3 - me)) * 100;
}

/* Negamax with alpha-beta; the best root move is written through best. */
static int search(unsigned char me, unsigned char depth, int alpha, int beta,
                  unsigned char *best)
{
    unsigned char saved[64];
    unsigned char i, c, other = 3 - me, any = 0;
    int score;

    if (depth == 0)
        return board_evaluate(me);
    for (i = 0; i < 64; i++) {
        c = ORDER[i];
        if (!board_is_legal(c, me))
            continue;
        memcpy(saved, board, 64);
        board_play(c, me);
        score = -search(other, depth - 1, -beta, -alpha, 0);
        memcpy(board, saved, 64);
        if (!any || score > alpha) {
            if (best)
                *best = c;
            if (score > alpha)
                alpha = score;
        }
        any = 1;
        if (alpha >= beta)
            break;
    }
    if (any)
        return alpha;
    if (!board_has_move(other))
        return final_score(me);
    return -search(other, depth - 1, -beta, -alpha, 0);   /* pass */
}

unsigned char cpu_choose(unsigned char me, unsigned char *cell)
{
    if (cpu_level <= 1)
        return greedy(me, cell);
    if (!board_has_move(me))
        return 0;
    search(me, cpu_level, -30000, 30000, cell);
    return 1;
}
