/* Othello voor de Philips P2000C -- play against the computer, in Dutch.
 *
 * The screen is the terminal board's 512x252 high-res mode. Everything on
 * the board (grid, frame, discs, coordinate glyphs, legal-move hints, the
 * cursor) is composed in a framebuffer; the first frame goes out as one bulk
 * write, after that only the cells whose appearance changed are re-sent as
 * small rectangles. The score panel on the right uses the 64x21 text plane.
 *
 * Keys: cursor keys or W/A/S/D move, RETURN or SPACE places, N restarts,
 * H shows the help screen (in plain text mode), Q asks before quitting.
 * The human plays Black and moves first; the difficulty (1-3) is chosen
 * at the start.
 * Dots have a 3:5 pitch on the CRT, so a 40x24-dot cell is square.
 */
#include "video.h"
#include "board.h"
#include "cpu.h"
#include "sprites.h"
#include "version.h"

/* Board geometry in dots; the board's left edge sits on byte 1 (x = 8). */
#define BOARD_TOP   12
#define CELL_H      24
#define CELL_BYTES  5
#define LEFT_BYTE   1
#define BOARD_LINES (8 * CELL_H)
#define FRAME_GAP   2                       /* outer frame two dots outside the grid */

/* Text panel (64-column text plane), right of the board. */
#define PANEL_ICON_BYTE 45
#define PANEL_COL       48
#define ROW_STATUS      9
#define ROW_NOTE        10
#define ROW_CURSOR      11
#define ROW_MOVE        13
#define BLANK16         "                "

/* Keyboard codes sent by the P2000C for the cursor keys (WordStar set). */
#define KEY_LEFT  0x15
#define KEY_RIGHT 0x06
#define KEY_UP    0x1A
#define KEY_DOWN  0x0A
#define KEY_CR    0x0D
#define BEL       0x07

/* Character-ROM glyphs used by the text-mode screens. */
#define CH_BLOCK 0x9F                       /* full 8x12 block */
#define CH_H     0xD0                       /* box drawing: single lines */
#define CH_V     0xFA
#define CH_TL    0xA9
#define CH_TR    0xB9
#define CH_BL    0xAA
#define CH_BR    0xBA
#define ESC      27

/* Appearance bits: low nibble disc colour, plus hint and cursor flags. */
#define SHOW_HINT   0x10
#define SHOW_CURSOR 0x20
#define SHOW_NONE   0xFF                    /* forces a redraw */
#define NO_CURSOR   0xFF                    /* cursor hidden during the computer's turn */

static unsigned char shown[64];             /* appearance last sent per cell */
static unsigned char to_move;
static unsigned char cursor;
static unsigned char game_over;
static unsigned char move_number;

/* --- drawing ---------------------------------------------------------------- */

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
    if (look == EMPTY && !game_over && to_move == BLACK && board_is_legal(cell, to_move))
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
    if (look & 0x0F)
        video_blit((look & 0x0F) == BLACK ? sprite_black : sprite_white,
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
static void sync_cells(void)
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

static void draw_board(void)
{
    unsigned char cell;
    video_clear();
    draw_frame();
    draw_labels();
    for (cell = 0; cell < 64; cell++)
        draw_cell(cell);
    /* score icons beside the text panel rows 5 and 6 (12 dots per text row) */
    video_blit(icon_black, (5 * 12 + 1) * FB_LINE + PANEL_ICON_BYTE, WH(2, 10));
    video_blit(icon_white, (6 * 12 + 1) * FB_LINE + PANEL_ICON_BYTE, WH(2, 10));
}

/* --- text panel --------------------------------------------------------------- */

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

static const char *name_of(unsigned char colour)
{
    return colour == BLACK ? "Zwart" : "Wit";
}

static void show_scores(void)
{
    con_at(ROWCOL(5, PANEL_COL + 14)); put_number(board_count(BLACK));
    con_at(ROWCOL(6, PANEL_COL + 14)); put_number(board_count(WHITE));
    con_at(ROWCOL(ROW_MOVE, PANEL_COL + 8)); put_number(move_number);
}

static void show_cursor_name(void)
{
    con_at(ROWCOL(ROW_CURSOR, PANEL_COL + 8));
    if (cursor == NO_CURSOR)
        con_puts("--");
    else
        put_cell_name(cursor);
}

/* The note row (row 10) is remembered so the help screen can restore it. */
static const char *note_text = "";
static unsigned char note_cell = NO_CURSOR;     /* White's last move, if that is the note */

static void show_note(const char *note)
{
    note_text = note;
    note_cell = NO_CURSOR;
    con_at(ROWCOL(ROW_NOTE, PANEL_COL));
    con_puts(note);
    con_puts(BLANK16);
}

static void show_cpu_move(unsigned char cell)
{
    note_cell = cell;
    con_at(ROWCOL(ROW_NOTE, PANEL_COL));        /* "Wit speelt E3" */
    con_puts("Wit speelt ");
    put_cell_name(cell);
    con_puts("   ");
}

static void restore_note(void)
{
    if (note_cell != NO_CURSOR)
        show_cpu_move(note_cell);
    else
        show_note(note_text);
}

/* Status line written last after every update, so it doubles as a
 * display-complete marker (the headless tests wait for it). */
static void show_status(const char *status)
{
    con_at(ROWCOL(ROW_STATUS, PANEL_COL));
    con_puts(status);
    con_puts(BLANK16);
}

static void announce_turn(void)
{
    con_at(ROWCOL(ROW_STATUS, PANEL_COL));
    con_puts(name_of(to_move));
    con_puts(" aan zet        ");
}

static void announce_result(void)
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

static void draw_panel(void)
{
    con_at(ROWCOL(1, PANEL_COL));  con_puts("O T H E L L O");
    con_at(ROWCOL(2, PANEL_COL));  con_puts("Philips P2000C");
    con_at(ROWCOL(5, PANEL_COL));  con_puts("Zwart (u)");
    con_at(ROWCOL(6, PANEL_COL));  con_puts("Wit (P2000C)");
    con_at(ROWCOL(ROW_CURSOR, PANEL_COL)); con_puts("Cursor  ");
    con_at(ROWCOL(12, PANEL_COL)); con_puts("Stip=geldige zet");
    con_at(ROWCOL(ROW_MOVE, PANEL_COL)); con_puts("Zet     ");
    con_at(ROWCOL(16, PANEL_COL)); con_puts("Pijltjes of WASD");
    con_at(ROWCOL(17, PANEL_COL)); con_puts("RETURN  zetten");
    con_at(ROWCOL(18, PANEL_COL)); con_puts("N nieuw Q stop");
    con_at(ROWCOL(19, PANEL_COL)); con_puts("H hulp");
    show_scores();
    show_cursor_name();
}

/* --- game flow ---------------------------------------------------------------- */

static void new_game(void)
{
    unsigned char cell;
    board_init();
    for (cell = 0; cell < 64; cell++)
        shown[cell] = SHOW_NONE;
    to_move = BLACK;
    game_over = 0;
    move_number = 1;
    cursor = board_first_move(BLACK);
}

/* Brings board, scores and cursor up to date, then the status line.
 * A null note leaves the note row as it is. */
static void refresh(const char *note)
{
    cursor = (!game_over && to_move == BLACK) ? board_first_move(BLACK) : NO_CURSOR;
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
            show_cpu_move(cell);
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

/* --- help screen ---------------------------------------------------------------- */

static const char *const HELP[] = {
    "OTHELLO v" VERSION " voor de Philips P2000C" "                  gecompileerd " BUILD_DATE,
    REPO_URL,
    "",
    "SPELREGELS",
    "  Zwart (u) begint. Om de beurt legt elke speler een schijf op een leeg veld.",
    "  Een zet is alleen geldig als de nieuwe schijf een of meer schijven van de",
    "  tegenstander insluit tussen zichzelf en een andere eigen schijf, in een",
    "  rechte lijn: horizontaal, verticaal of diagonaal. Alle ingesloten schijven",
    "  worden omgedraaid en zijn dan van u.",
    "  Wie geen geldige zet heeft, moet passen. Als geen van beide spelers nog een",
    "  zet heeft, of het bord vol is, is het spel afgelopen. Wie dan de meeste",
    "  schijven heeft, wint.",
    "",
    "TOETSEN",
    "  Pijltjes of W A S D   cursor verplaatsen      RETURN of spatie   schijf leggen",
    "  H                     dit hulpscherm          N                  nieuw spel",
    "  Q                     stoppen (met bevestiging)",
    "",
    "NIVEAUS",
    "  1  licht     de computer pakt de zet die de meeste schijven omdraait",
    "  2  normaal   de computer kijkt twee zetten vooruit",
    "  3  zwaar     de computer kijkt drie zetten vooruit",
    "",
    "Druk op een toets om terug te keren naar het spel.",
};

/* Clears the 80x24 text screen and hides the blinking cursor. */
static void text_clear(void)
{
    con_at(ROWCOL(0, 0));
    conout(ESC); conout('k');
    conout(ESC); conout('c');
}

/* Prints the rules on the (already selected) text screen and waits for a key. */
static void help_page(void)
{
    unsigned char row;
    text_clear();
    for (row = 0; row < sizeof HELP / sizeof HELP[0]; row++) {
        con_at(ROWCOL(row, 0));
        con_puts(HELP[row]);
    }
    conin();
}

/* Shows the rules from the game, then restores the board. Leaving graphics
 * mode clears the terminal's picture, but the framebuffer in RAM is intact,
 * so the return is one full-frame bulk write. */
static void help_screen(void)
{
    video_text();
    help_page();
    video_graphics();
    draw_panel();
    video_flush_rows(WH(0, FB_LINES));
    if (game_over)
        announce_result();
    else {
        restore_note();
        announce_turn();
    }
}

/* --- start screen ------------------------------------------------------------ */

/* OTHELLO in a five-row block font, 38 pixels wide; every pixel becomes two
 * block characters, which is close to square on the CRT. */
static const char *const TITLE[5] = {
    " ###  ##### #   # #### #    #     ### ",
    "#   #   #   #   # #    #    #    #   #",
    "#   #   #   ##### ###  #    #    #   #",
    "#   #   #   #   # #    #    #    #   #",
    " ###    #   #   # #### #### ####  ### ",
};

static void put_repeat(unsigned char ch, unsigned char count)
{
    while (count--)
        conout(ch);
}

/* A single-line box from (row, col), width and height in cells. */
static void draw_box(unsigned char row, unsigned char col, unsigned char width, unsigned char height)
{
    unsigned char r;
    con_at(ROWCOL(row, col));
    conout(CH_TL); put_repeat(CH_H, width - 2); conout(CH_TR);
    for (r = row + 1; r < row + height - 1; r++) {
        con_at(ROWCOL(r, col)); conout(CH_V);
        con_at(ROWCOL(r, col + width - 1)); conout(CH_V);
    }
    con_at(ROWCOL(row + height - 1, col));
    conout(CH_BL); put_repeat(CH_H, width - 2); conout(CH_BR);
}

static void draw_start_screen(void)
{
    unsigned char row;
    const char *pixel;

    text_clear();
    draw_box(0, 0, 80, 23);                  /* row 23 stays empty: writing its last cell scrolls */
    for (row = 0; row < 5; row++) {
        con_at(ROWCOL(2 + row, 2));
        for (pixel = TITLE[row]; *pixel; pixel++) {
            conout(*pixel == '#' ? CH_BLOCK : ' ');
            conout(*pixel == '#' ? CH_BLOCK : ' ');
        }
    }
    con_at(ROWCOL(8, 29));  con_puts("voor de Philips P2000C");
    con_at(ROWCOL(9, 12));  con_puts("versie " VERSION "   -   " REPO_URL);

    draw_box(11, 16, 48, 7);
    con_at(ROWCOL(12, 20)); con_puts("Kies de sterkte van de computer:");
    con_at(ROWCOL(14, 20)); con_puts("1  Licht     pakt de meeste schijven");
    con_at(ROWCOL(15, 20)); con_puts("2  Normaal   kijkt twee zetten vooruit");
    con_at(ROWCOL(16, 20)); con_puts("3  Zwaar     kijkt drie zetten vooruit");

    con_at(ROWCOL(19, 12)); con_puts("U speelt met zwart en begint, de P2000C speelt met wit.");
    con_at(ROWCOL(21, 9));  con_puts("1, 2 of 3: spelen       H: spelregels       Q: terug naar CP/M");
}

/* Text-mode start screen; returns the chosen level, or 0 to leave the program. */
static unsigned char start_screen(void)
{
    unsigned char key;
    draw_start_screen();
    for (;;) {
        key = conin();
        if (key >= '1' && key <= '3')
            return key - '0';
        if (key == 'q' || key == 'Q')
            return 0;
        if (key == 'h' || key == 'H') {
            help_page();
            draw_start_screen();
        }
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
static unsigned char play(void)
{
    unsigned char key;

    new_game();
    draw_board();
    video_graphics();
    draw_panel();
    video_flush_rows(WH(0, FB_LINES));
    announce_turn();

    for (;;) {
        key = conin();
        if (key >= 'A' && key <= 'Z')
            key += 'a' - 'A';
        switch (key) {
        case KEY_LEFT:  case 'a': move_cursor(-1, 0); break;
        case KEY_RIGHT: case 'd': move_cursor(1, 0);  break;
        case KEY_UP:    case 'w': move_cursor(0, -1); break;
        case KEY_DOWN:  case 's': move_cursor(0, 1);  break;
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

int main(void)
{
    unsigned char level;

    conout(ESC); conout('c');
    while ((level = start_screen()) != 0) {
        cpu_level = level;
        level = play();
        video_text();
        if (!level)
            break;
    }
    text_clear();
    conout(ESC); conout('C');                /* CP/M gets its cursor back */
    return 0;
}
