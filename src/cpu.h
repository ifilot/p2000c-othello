/* cpu.h -- computer player. */
#ifndef CPU_H
#define CPU_H

/* 1 = greedy flip count (after the P2000M version), 2 and 3 = search depth. */
extern unsigned char cpu_level;

/* Chooses a legal move for the colour; returns 0 when it must pass. */
extern unsigned char cpu_choose(unsigned char me, unsigned char *cell);

#endif
