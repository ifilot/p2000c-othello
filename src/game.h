/* SPDX-License-Identifier: GPL-3.0-only */
/* game.h -- game state shared by the modules, and the game flow. */
#ifndef GAME_H
#define GAME_H

#define NO_CURSOR 0xFF                      /* cursor hidden (computer's turn, demo) */

extern unsigned char to_move;               /* BLACK or WHITE */
extern unsigned char cursor;                /* human's cursor cell, or NO_CURSOR */
extern unsigned char game_over;
extern unsigned char move_number;           /* 1-based, counts both colours */
extern unsigned char demo;                  /* both colours played by the computer */

/* One game against the computer at cpu_level; returns 1 to go back to the
 * start screen (N), 0 to leave the program (Q confirmed). */
extern unsigned char play(void);

/* A demo game between two computer players; any key ends it. */
extern void demo_game(void);

/* Restores the game screen after a text-mode interlude (help, screen saver). */
extern void redraw_game_screen(void);

#endif
