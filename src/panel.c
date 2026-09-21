/* SPDX-License-Identifier: GPL-3.0-only */
/* panel.c -- the text panel: title, scores, status, notes and key help.
 *
 * Every line is padded to the panel's 16 columns so a shorter text overwrites
 * a longer one. The status line is written last after every update.
 */
#include "video.h"
#include "board.h"
#include "game.h"
#include "panel.h"
#include "clock.h"

#define BLANK16 "                "
#define CLOCK_MAX 35999u                    /* 9:59:59 */

static void put_number(unsigned char n)
{
    conout(n >= 10 ? '0' + n / 10 : ' ');
    conout('0' + n % 10);
}

static void put_cell_name(unsigned char cell)
{
    conout('A' + (cell & 7));
    conout('1' + (cell >> 3));
}

const char *name_of(unsigned char colour)
{
    return colour == BLACK ? "Zwart" : "Wit";
}

void show_scores(void)
{
    con_at(ROWCOL(5, PANEL_COL + 14)); put_number(board_count(BLACK));
    con_at(ROWCOL(6, PANEL_COL + 14)); put_number(board_count(WHITE));
    con_at(ROWCOL(ROW_MOVE, PANEL_COL + 8)); put_number(move_number);
}

/* "Tijd  0:12:34": hours, minutes and seconds, capped at 9:59:59. */
void show_clock(void)
{
    unsigned int s = clock_seconds(), h, m;
    if (!clock_available)
        return;
    if (s > CLOCK_MAX)
        s = CLOCK_MAX;
    h = s / 3600;
    m = (s / 60) % 60;
    s %= 60;
    con_at(ROWCOL(ROW_CLOCK, PANEL_COL + 6));
    conout('0' + h); conout(':');
    conout('0' + m / 10); conout('0' + m % 10); conout(':');
    conout('0' + s / 10); conout('0' + s % 10);
}

void show_cursor_name(void)
{
    if (demo)
        return;
    con_at(ROWCOL(ROW_CURSOR, PANEL_COL + 8));
    if (cursor == NO_CURSOR)
        con_puts("--");
    else
        put_cell_name(cursor);
}

/* The note row (row 10) is remembered so the help screen can restore it:
 * either a text, or a computer move (colour + cell). */
static const char *note_text = "";
static unsigned char note_cell = NO_CURSOR;
static unsigned char note_colour = WHITE;

void show_note(const char *note)
{
    note_text = note;
    note_cell = NO_CURSOR;
    con_at(ROWCOL(ROW_NOTE, PANEL_COL));
    con_puts(note);
    con_puts(BLANK16);
}

void show_cpu_move(unsigned char colour, unsigned char cell)
{
    note_cell = cell;
    note_colour = colour;
    con_at(ROWCOL(ROW_NOTE, PANEL_COL));        /* "Wit speelt E3" */
    con_puts(name_of(colour));
    con_puts(" speelt ");
    put_cell_name(cell);
    con_puts("     ");
}

void restore_note(void)
{
    if (note_cell != NO_CURSOR)
        show_cpu_move(note_colour, note_cell);
    else
        show_note(note_text);
}

/* Status line written last after every update, so it doubles as a
 * display-complete marker (the headless tests wait for it). */
void show_status(const char *status)
{
    con_at(ROWCOL(ROW_STATUS, PANEL_COL));
    con_puts(status);
    con_puts(BLANK16);
}

void announce_turn(void)
{
    con_at(ROWCOL(ROW_STATUS, PANEL_COL));
    con_puts(name_of(to_move));
    con_puts(" aan zet        ");
}

void announce_result(void)
{
    unsigned char b = board_count(BLACK), w = board_count(WHITE);
    show_note("Spel afgelopen");
    con_at(ROWCOL(ROW_STATUS, PANEL_COL));
    if (b == w)
        con_puts("Gelijkspel      ");
    else {
        con_puts(name_of(b > w ? BLACK : WHITE));
        con_puts(" wint       ");
    }
}

void draw_panel(void)
{
    con_at(ROWCOL(1, PANEL_COL));  con_puts("O T H E L L O");
    con_at(ROWCOL(2, PANEL_COL));  con_puts("Philips P2000C");
    if (demo) {
        con_at(ROWCOL(5, PANEL_COL));  con_puts("Zwart niv.2");
        con_at(ROWCOL(6, PANEL_COL));  con_puts("Wit   niv.3");
        con_at(ROWCOL(ROW_MOVE, PANEL_COL)); con_puts("Zet     ");
        if (clock_available) {
            con_at(ROWCOL(ROW_CLOCK, PANEL_COL)); con_puts("Tijd    ");
        }
        con_at(ROWCOL(16, PANEL_COL)); con_puts("DEMO");
        con_at(ROWCOL(17, PANEL_COL)); con_puts("computer tegen");
        con_at(ROWCOL(18, PANEL_COL)); con_puts("computer");
        con_at(ROWCOL(19, PANEL_COL)); con_puts("Toets: stoppen");
        show_scores();
        show_clock();
        return;
    }
    con_at(ROWCOL(5, PANEL_COL));  con_puts("Zwart (u)");
    con_at(ROWCOL(6, PANEL_COL));  con_puts("Wit (P2000C)");
    con_at(ROWCOL(ROW_CURSOR, PANEL_COL)); con_puts("Cursor  ");
    con_at(ROWCOL(12, PANEL_COL)); con_puts("Stip=geldige zet");
    con_at(ROWCOL(ROW_MOVE, PANEL_COL)); con_puts("Zet     ");
    if (clock_available) {
        con_at(ROWCOL(ROW_CLOCK, PANEL_COL)); con_puts("Tijd    ");
    }
    con_at(ROWCOL(16, PANEL_COL)); con_puts("Pijltjes of WASD");
    con_at(ROWCOL(17, PANEL_COL)); con_puts("RETURN  zetten");
    con_at(ROWCOL(18, PANEL_COL)); con_puts("N nieuw Q stop");
    con_at(ROWCOL(19, PANEL_COL)); con_puts("H hulp");
    show_scores();
    show_clock();
    show_cursor_name();
}
