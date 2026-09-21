/* SPDX-License-Identifier: GPL-3.0-only */
/* screens.c -- the text-mode screens and the title picture.
 *
 * The start and help screens use the plain 80x24 text mode (instant); the
 * title picture is a 512x252 bitmap kept run-length encoded in the binary
 * (splash.h), unpacked into the framebuffer and uploaded sparsely.
 */
#include "video.h"
#include "board.h"
#include "cpu.h"
#include "game.h"
#include "screen.h"
#include "panel.h"
#include "screens.h"
#include "saver.h"
#include "splash.h"
#include "version.h"

/* Character-ROM glyphs used by the text-mode screens. */
#define CH_BLOCK 0x9F                       /* full 8x12 block */
#define CH_H     0xD0                       /* box drawing: single lines */
#define CH_V     0xFA
#define CH_TL    0xA9
#define CH_TR    0xB9
#define CH_BL    0xAA
#define CH_BR    0xBA
#define ESC      27

/* --- help ------------------------------------------------------------------------ */

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
    "  D (startscherm)       demo: de computer speelt tegen zichzelf",
    "",
    "NIVEAUS",
    "  1  licht     de computer pakt de zet die de meeste schijven omdraait",
    "  2  normaal   de computer kijkt twee zetten vooruit",
    "  3  zwaar     de computer kijkt drie zetten vooruit",
    "",
    "Druk op een toets om terug te keren naar het spel.",
};

/* Clears the 80x24 text screen and hides the blinking cursor. */
void text_clear(void)
{
    con_at(ROWCOL(0, 0));
    conout(ESC); conout('k');
    conout(ESC); conout('c');
}

/* Prints the rules on the (already selected) text screen. */
static void draw_help_page(void)
{
    unsigned char row;
    text_clear();
    for (row = 0; row < sizeof HELP / sizeof HELP[0]; row++) {
        con_at(ROWCOL(row, 0));
        con_puts(HELP[row]);
    }
}

static void help_page(void)
{
    draw_help_page();
    wait_key(draw_help_page);
}

/* Shows the rules from the game, then restores the board. Leaving graphics
 * mode clears the terminal's picture, but the framebuffer in RAM is intact,
 * so the return costs one flush_frame(). */
void help_screen(void)
{
    video_text();
    help_page();
    redraw_game_screen();
}

/* --- title picture ----------------------------------------------------------- */

/* Unpacks the run-length encoded title bitmap into the framebuffer. */
static void unpack_splash(void)
{
    const unsigned char *in = splash_rle;
    unsigned char *out = framebuffer;
    unsigned int left = SPLASH_RLE_SIZE;
    unsigned char n;
    while (left) {
        n = in[0];
        while (n--)
            *out++ = in[1];
        in += 2;
        left -= 2;
    }
}

static void draw_splash(void)
{
    unpack_splash();
    video_graphics();
    flush_sparse();
}

/* Title picture in graphics mode; returns after any key. */
void splash_screen(void)
{
    draw_splash();
    wait_key(draw_splash);
    video_text();
}

/* --- start screen -------------------------------------------------------------- */

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
    con_at(ROWCOL(21, 6));  con_puts("1, 2 of 3: spelen    D: demo    H: spelregels    Q: terug naar CP/M");
}

/* Text-mode start screen; returns the chosen level, DEMO for a demo game, or
 * 0 to leave the program. */
unsigned char start_screen(void)
{
    unsigned char key;
    draw_start_screen();
    for (;;) {
        key = wait_key(draw_start_screen);
        if (key >= '1' && key <= '3')
            return key - '0';
        if (key == 'd' || key == 'D')
            return START_DEMO;
        if (key == 'q' || key == 'Q')
            return 0;
        if (key == 'h' || key == 'H') {
            help_page();
            draw_start_screen();
        }
    }
}
