/* SPDX-License-Identifier: GPL-3.0-only */
/* Othello voor de Philips P2000C -- play against the computer, in Dutch.
 *
 * Title picture, start screen (level 1-3, demo, help, quit), then games
 * until the player leaves. Modules:
 *   game.c    state, turn cycle, demo           screen.c  board picture and uploads
 *   panel.c   text panel                        screens.c start/help screens, title
 *   saver.c   key waits with the CRT screen saver   clock.c   game clock (BIOS ticks)
 *   board.c   rules (hot paths in rules.asm)    cpu.c     computer player
 *   video.asm framebuffer primitives, ESC r uploads, BIOS console I/O
 *
 * The board uses the terminal's 512x252 high-resolution mode with the 64x21
 * text plane for the panel; the other screens use the 80x24 text mode.
 */
#include "video.h"
#include "cpu.h"
#include "game.h"
#include "screens.h"
#include "clock.h"

#define ESC 27

int main(void)
{
    unsigned char level;

    conout(ESC); conout('c');                /* no blinking text cursor */
    clock_probe();
    splash_screen();
    while ((level = start_screen()) != 0) {
        if (level == START_DEMO) {
            demo_game();
            video_text();
            continue;
        }
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
