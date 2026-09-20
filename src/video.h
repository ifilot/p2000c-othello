/* video.h -- framebuffer and terminal-board primitives (see video.asm). */
#ifndef VIDEO_H
#define VIDEO_H

#define FB_LINE 64          /* bytes per framebuffer line */
#define FB_LINES 252

extern unsigned char framebuffer[FB_LINE * FB_LINES];

/* Pack (width in bytes, rows) or (row, column) into one word argument. */
#define WH(w, h)      ((unsigned int)(w) | ((unsigned int)(h) << 8))
#define ROWCOL(r, c)  ((unsigned int)(r) | ((unsigned int)(c) << 8))
#define COLROW(c, r)  ((unsigned int)(c) | ((unsigned int)(r) << 8))

extern void video_clear(void);
extern void video_fill(unsigned int offset, unsigned int value, unsigned int count);
extern void video_or_col(unsigned int offset, unsigned int mask, unsigned int count);
extern void video_blit(const unsigned char *sprite, unsigned int offset, unsigned int wh);
extern void video_xor(const unsigned char *sprite, unsigned int offset, unsigned int wh);
extern void video_flush_rows(unsigned int first_count) __z88dk_fastcall;
extern void video_flush_rect(unsigned int col_row, unsigned int wh);
extern void video_graphics(void);
extern void video_text(void);

extern void conout(unsigned int c) __z88dk_fastcall;
extern unsigned char conin(void);
extern void con_puts(const char *s) __z88dk_fastcall;
extern void con_at(unsigned int row_col) __z88dk_fastcall;

#endif
