/* SPDX-License-Identifier: GPL-3.0-only */
/* game.c -- game state, the human/computer turn cycle, the demo game.
 *
 * The human plays Black and moves first; White is the computer (cpu.c).
 * After every change the board is brought up to date through the screen
 * module and the panel through the panel module; the status line is always
 * written last, so it doubles as a display-complete marker for the tests.
 */
#include "video.h"
#include "board.h"
#include "cpu.h"
#include "game.h"
#include "screen.h"
#include "panel.h"
#include "screens.h"
#include "saver.h"
#include "clock.h"

unsigned char to_move;
unsigned char cursor;
unsigned char game_over;
unsigned char move_number;
unsigned char demo;

#define DEMO_BLACK_LEVEL 2
#define DEMO_WHITE_LEVEL 3
#define DEMO_PAUSE       6000               /* pause_or_key() iterations, about a second at 4 MHz */

/* Cursor keys. The P2000C keyboard's cursor quadrant emits the WordStar
 * diamond (^S ^D ^E ^X, as P2EDIT and SuperCalc expect); the graphical
 * emulator sends the terminal's own cursor-control bytes instead, so both
 * sets are accepted. */
#define KEY_LEFT   0x13                     /* ^S */
#define KEY_RIGHT  0x04                     /* ^D */
#define KEY_UP     0x05                     /* ^E */
#define KEY_DOWN   0x18                     /* ^X */
#define KEY_LEFT2  0x15
#define KEY_RIGHT2 0x06
#define KEY_UP2    0x1A
#define KEY_DOWN2  0x0A
#define KEY_CR     0x0D
#define BEL        0x07

/* --- turn cycle ---------------------------------------------------------------- */

/* Idle hook while waiting for the player: keeps the clock display current. */
static void tick_clock(void)
{
    if (clock_update())
        show_clock();
}

/* Standard opening position; the picture is rebuilt by draw_board() afterwards. */
static void new_game(void)
{
    board_init();
    clock_reset();
    to_move = BLACK;
    game_over = 0;
    move_number = 1;
    cursor = demo ? NO_CURSOR : board_first_move(BLACK);
}

/* Brings board, scores and cursor up to date, then the status line.
 * A null note leaves the note row as it is. */
static void refresh(const char *note)
{
    cursor = (!game_over && !demo && to_move == BLACK) ? board_first_move(BLACK) : NO_CURSOR;
    sync_cells();
    show_scores();
    show_cursor_name();
    if (game_over)
        announce_result();
    else {
        if (note)
            show_note(note);
        announce_turn();
    }
}

/* Applies a legal move for to_move and advances the turn; returns the note to show. */
static const char *advance(unsigned char cell)
{
    unsigned char me = to_move, other = 3 - to_move;
    board_play(cell, me);
    move_number++;
    if (board_has_move(other)) {
        to_move = other;
        return "";
    }
    if (board_has_move(me))
        return other == BLACK ? "Zwart past" : "Wit past";
    game_over = 1;
    clock_freeze();
    return "";
}

/* The computer moves as White until it is Black's turn or the game ends. */
static void cpu_turn(void)
{
    unsigned char cell;
    const char *note;
    while (!game_over && to_move == WHITE) {
        show_status("Wit denkt...");
        cpu_choose(WHITE, &cell);           /* to_move == WHITE implies a legal move */
        note = advance(cell);
        if (*note == '\0') {
            show_cpu_move(WHITE, cell);
            note = 0;
        }
        refresh(note);
    }
}

/* Plays the human's move at the cursor; returns 0 if it was not legal. */
static unsigned char play_cursor(void)
{
    const char *note;
    if (game_over || !board_is_legal(cursor, to_move))
        return 0;
    show_status("Wit denkt...");           /* replaces the marker before any flushing */
    note = advance(cursor);
    refresh(note);
    cpu_turn();
    return 1;
}

static void move_cursor(signed char dcol, signed char drow)
{
    signed char col = (cursor & 7) + dcol, row = (cursor >> 3) + drow;
    if (col < 0 || col > 7 || row < 0 || row > 7)
        return;
    cursor = row * 8 + col;
    sync_cells();
    show_cursor_name();
}

/* --- demo: computer against computer ------------------------------------------ */

/* Waits roughly a second (at 4 MHz); a key pressed meanwhile is consumed and
 * reported. */
static unsigned char pause_or_key(void)
{
    unsigned int i;
    for (i = 0; i < DEMO_PAUSE; i++) {
        if (conready()) {
            conin();
            return 1;
        }
        if ((i & 511) == 0)
            tick_clock();
    }
    return 0;
}

/* One demo game; returns when it is over and a key was pressed, or when a
 * key interrupts it. The pressed key is consumed. */
void demo_game(void)
{
    unsigned char cell, mover;
    const char *note;

    demo = 1;
    new_game();
    draw_board();
    video_graphics();
    draw_panel();
    flush_frame();
    announce_turn();

    while (!game_over) {
        cpu_level = to_move == BLACK ? DEMO_BLACK_LEVEL : DEMO_WHITE_LEVEL;
        con_at(ROWCOL(ROW_STATUS, PANEL_COL));
        con_puts(name_of(to_move));
        con_puts(" denkt...       ");
        cpu_choose(to_move, &cell);         /* to_move always has a legal move */
        mover = to_move;
        note = advance(cell);
        show_cpu_move(mover, cell);
        if (*note != '\0') {
            /* a pass: show it in the status line, the move stays in the note */
            sync_cells();
            show_scores();
            show_status(note);
        } else
            refresh(0);
        if (pause_or_key())
            break;
    }
    if (!game_over)
        show_status("Demo gestopt");
    wait_key_idle(redraw_game_screen, tick_clock);   /* the demo is over: the saver may run now */
    demo = 0;
}

/* --- one game against the computer -------------------------------------------- */

/* Rebuilds the whole game screen from the framebuffer and the game state,
 * after the help page or the screen saver (both leave graphics mode). */
void redraw_game_screen(void)
{
    video_graphics();
    draw_panel();
    flush_frame();
    if (game_over)
        announce_result();
    else if (demo)
        show_status("Demo gestopt");
    else {
        restore_note();
        announce_turn();
    }
}

/* "Stoppen? (J/N)": returns 1 when the player confirms. */
static unsigned char confirm_quit(void)
{
    unsigned char key;
    show_status("Stoppen? (J/N)");
    key = conin();
    if (key == 'j' || key == 'J' || key == 'y' || key == 'Y')
        return 1;
    if (game_over)
        announce_result();
    else
        announce_turn();
    return 0;
}

/* One game; returns 1 to go back to the start screen, 0 to leave the program. */
unsigned char play(void)
{
    unsigned char key;

    new_game();
    draw_board();
    video_graphics();
    draw_panel();
    flush_frame();
    announce_turn();

    for (;;) {
        key = wait_key_idle(redraw_game_screen, tick_clock);
        if (key >= 'A' && key <= 'Z')
            key += 'a' - 'A';
        switch (key) {
        case KEY_LEFT:  case KEY_LEFT2:  case 'a': move_cursor(-1, 0); break;
        case KEY_RIGHT: case KEY_RIGHT2: case 'd': move_cursor(1, 0);  break;
        case KEY_UP:    case KEY_UP2:    case 'w': move_cursor(0, -1); break;
        case KEY_DOWN:  case KEY_DOWN2:  case 's': move_cursor(0, 1);  break;
        case KEY_CR:    case ' ':
            if (!play_cursor())
                conout(BEL);
            break;
        case 'h':
            help_screen();
            break;
        case 'n':
            return 1;
        case 'q':
            if (confirm_quit())
                return 0;
            break;
        }
    }
}
