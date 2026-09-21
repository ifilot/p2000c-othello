/* SPDX-License-Identifier: GPL-3.0-only */
/* panel.h -- the score/status panel on the 64x21 text plane, right of the board. */
#ifndef PANEL_H
#define PANEL_H

#define PANEL_COL  48
#define ROW_STATUS 9
#define ROW_NOTE   10
#define ROW_CURSOR 11
#define ROW_MOVE   13
#define ROW_CLOCK  14

extern const char *name_of(unsigned char colour);       /* "Zwart" / "Wit" */
extern void draw_panel(void);                          /* static texts, scores, cursor */
extern void show_scores(void);                         /* disc counts and move number */
extern void show_clock(void);                          /* elapsed game time, if a clock exists */
extern void show_cursor_name(void);
extern void show_note(const char *note);               /* row 10, remembered */
extern void show_cpu_move(unsigned char colour, unsigned char cell);   /* "Wit speelt E3" */
extern void restore_note(void);                        /* after the help screen */
extern void show_status(const char *status);           /* row 9, written last */
extern void announce_turn(void);                       /* "Zwart aan zet" */
extern void announce_result(void);                     /* "Wit wint" / "Gelijkspel" */

#endif
