; SPDX-License-Identifier: GPL-3.0-only
; video.asm -- framebuffer and terminal-board primitives for the P2000C.
;
; The 512x252 high-res picture is composed in a 16128-byte framebuffer
; (64 bytes per line, line 0 at the top, MSB = leftmost dot) and sent to the
; terminal board with ESC r bulk writes. All bytes go through BIOS CONOUT,
; because BDOS console output filters control characters and cannot send 0FFh.
;
; Calling convention (Z88DK, sdcc_iy): stack arguments are 16-bit words with
; the leftmost argument at SP+2 on entry; __z88dk_fastcall passes one word in
; HL; 8-bit results return in L. IX and IY are never touched.

SECTION code_user

PUBLIC _framebuffer
PUBLIC _video_clear
PUBLIC _video_fill
PUBLIC _video_or_col
PUBLIC _video_blit
PUBLIC _video_xor
PUBLIC _video_flush_rows
PUBLIC _video_flush_rect
PUBLIC _video_graphics
PUBLIC _video_text
PUBLIC _conout
PUBLIC _conin
PUBLIC _conready
PUBLIC _con_puts
PUBLIC _con_at

defc FB_SIZE = 16128
defc LINE    = 64
defc ESC     = 27
defc BIOS    = 0x0001           ; -> WBOOT; CONIN = +9, CONOUT = +0Ch

; --- framebuffer ------------------------------------------------------------

; void video_clear(void)
_video_clear:
        ld hl,_framebuffer
        ld de,_framebuffer+1
        ld bc,FB_SIZE-1
        ld (hl),0
        ldir
        ret

; void video_fill(unsigned int offset, unsigned int value, unsigned int count)
; Sets count consecutive framebuffer bytes starting at offset to value.
_video_fill:
        ld hl,2
        add hl,sp
        ld c,(hl)
        inc hl
        ld b,(hl)               ; BC = offset
        inc hl
        ld e,(hl)               ; E = value
        inc hl
        inc hl
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a                  ; HL = count
        ld a,h
        or l
        ret z
        push hl
        ld hl,_framebuffer
        add hl,bc
        pop bc                  ; HL = destination, BC = count
fill_loop:
        ld (hl),e
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,fill_loop
        ret

; void video_or_col(unsigned int offset, unsigned int mask, unsigned int count)
; ORs mask into one byte on each of count (1..255) consecutive lines.
_video_or_col:
        ld hl,2
        add hl,sp
        ld c,(hl)
        inc hl
        ld b,(hl)               ; BC = offset
        inc hl
        ld e,(hl)               ; E = mask
        inc hl
        inc hl
        ld d,(hl)               ; D = count
        ld hl,_framebuffer
        add hl,bc
        ld bc,LINE
orcol_loop:
        ld a,(hl)
        or e
        ld (hl),a
        add hl,bc
        dec d
        jr nz,orcol_loop
        ret

; Shared argument decoding for the blitters (called, so arguments sit at SP+4):
; HL = sprite, DE = framebuffer destination, B = rows, width stored aside.
blit_args:
        ld hl,4
        add hl,sp
        ld e,(hl)
        inc hl
        ld d,(hl)               ; DE = sprite
        inc hl
        ld c,(hl)
        inc hl
        ld b,(hl)               ; BC = offset
        inc hl
        ld a,(hl)
        ld (blit_width),a       ; bytes per sprite row
        inc hl
        ld a,(hl)               ; rows
        ld hl,_framebuffer
        add hl,bc
        ex de,hl
        ld b,a
        ret

; void video_blit(const unsigned char *sprite, unsigned int offset, unsigned int wh)
; ORs a sprite of (wh & 0xff) bytes per row and (wh >> 8) rows onto the framebuffer.
_video_blit:
        call blit_args
blit_row:
        push bc
        push de
        ld a,(blit_width)
        ld c,a
blit_byte:
        ld a,(de)
        or (hl)
        ld (de),a
        inc hl
        inc de
        dec c
        jr nz,blit_byte
        pop de
        ex de,hl
        ld bc,LINE
        add hl,bc
        ex de,hl
        pop bc
        djnz blit_row
        ret

; void video_xor(const unsigned char *sprite, unsigned int offset, unsigned int wh)
; Like video_blit but XORs, so drawing the same sprite twice removes it.
_video_xor:
        call blit_args
xor_row:
        push bc
        push de
        ld a,(blit_width)
        ld c,a
xor_byte:
        ld a,(de)
        xor (hl)
        ld (de),a
        inc hl
        inc de
        dec c
        jr nz,xor_byte
        pop de
        ex de,hl
        ld bc,LINE
        add hl,bc
        ex de,hl
        pop bc
        djnz xor_row
        ret

; void video_flush_rows(unsigned int first_count)   fastcall: L = first line, H = lines
; Sends whole lines [first, first+count) with ESC r picture uploads. Two
; rules were established on real hardware: the byte count must not have a
; zero low byte (1000 bytes work, 1024 do not, and the excess arrives as
; text), and the terminal's picture RAM runs bottom-up -- consecutive bytes
; of an upload fill the given line and then the line ABOVE it. A multi-line
; upload therefore starts at the lowest line of its region and sends the
; framebuffer lines from bottom to top. Uploads are at most CHUNK_LINES
; lines and never a multiple of four lines (256n bytes).
defc CHUNK_LINES = 15

_video_flush_rows:
        ld a,h
        or a
        ret z
        ld e,h                  ; E = lines left
        ld a,l
        add a,h
        dec a
        ld d,a                  ; D = bottom line of the remaining region
rows_chunk:
        ld a,e
        cp CHUNK_LINES
        jr c,rows_size
        ld a,CHUNK_LINES
rows_size:
        ld c,a                  ; C = lines in this chunk
        and 3
        jr nz,rows_ok
        dec c                   ; multiple of four lines: send one line less
rows_ok:
        ld a,c
        ld (chunk_lines),a
        push de
        push bc
        ld a,ESC
        call conout_a
        ld a,'r'
        call conout_a
        xor a
        call conout_a           ; x low
        xor a
        call conout_a           ; x high
        pop bc
        pop de
        push de
        push bc
        ld a,251
        sub d
        call conout_a           ; y of the chunk's bottom line
        pop bc
        pop de
        push de
        push bc
        ld a,c                  ; count = lines * 64: low = (lines & 3) << 6, high = lines >> 2
        and 3
        rrca
        rrca
        call conout_a
        pop bc
        pop de
        push de
        push bc
        ld a,c
        rrca
        rrca
        and 0x3f
        call conout_a
        pop bc
        pop de
        ; HL = framebuffer + bottom line * 64; send C lines walking upward
        ld a,d
        and 3
        rrca
        rrca
        ld l,a
        ld a,d
        rrca
        rrca
        and 0x3f
        ld h,a
        ld bc,_framebuffer
        add hl,bc
        push de
        ld a,(chunk_lines)
        ld d,a                  ; D = lines to send
rows_line:
        ld b,LINE
rows_byte:
        ld a,(hl)
        call conout_a
        inc hl
        djnz rows_byte
        ld bc,-2*LINE           ; back to the start of the line above
        add hl,bc
        dec d
        jr nz,rows_line
        pop de
        ld a,(chunk_lines)
        ld c,a
        ld a,e                  ; advance upward by the chunk's lines
        sub c
        ld e,a
        ld a,d
        sub c
        ld d,a
        ld a,e
        or a
        jp nz,rows_chunk
        ret

; void video_flush_rect(unsigned int col_row, unsigned int wh)
; Sends a rectangle of (wh & 0xff) bytes per row and (wh >> 8) rows whose
; top-left byte is column (col_row & 0xff) on line (col_row >> 8), as one
; short ESC r write per row. Width must be 1..255 bytes.
_video_flush_rect:
        ld hl,2
        add hl,sp
        ld c,(hl)               ; C = byte column
        inc hl
        ld b,(hl)               ; B = first line
        inc hl
        ld e,(hl)               ; E = width in bytes
        inc hl
        ld d,(hl)               ; D = rows
        ld a,d
        or a
        ret z
        ; HL = framebuffer + line * 64 + column
        ld a,b
        and 3
        rrca
        rrca
        ld l,a
        ld a,b
        rrca
        rrca
        and 0x3f
        ld h,a
        push bc
        ld b,0
        add hl,bc
        ld bc,_framebuffer
        add hl,bc
        pop bc
rect_row:
        push bc
        push de
        push hl
        ld a,ESC
        call conout_a
        ld a,'r'
        call conout_a
        ld a,c                  ; x = column * 8
        rlca
        rlca
        rlca
        and 0xf8
        call conout_a           ; x low
        ld a,c
        rrca
        rrca
        rrca
        rrca
        rrca
        and 0x07
        call conout_a           ; x high
        ld a,251
        sub b
        call conout_a           ; y = 251 - line
        ld a,e
        call conout_a           ; count low
        xor a
        call conout_a           ; count high
        pop hl
        push hl
        ld b,e
rect_byte:
        ld a,(hl)
        call conout_a
        inc hl
        djnz rect_byte
        pop hl
        ld bc,LINE
        add hl,bc
        pop de
        pop bc
        inc b                   ; next line
        dec d
        jr nz,rect_row
        ret

; --- terminal -----------------------------------------------------------------

; void video_graphics(void)  -- ESC 3 high-res mode, hide the blinking text cursor,
;                               home and clear the text plane
_video_graphics:
        ld hl,seq_graphics
        jr puts_loop

; void video_text(void)      -- ESC 4 back to the 80x24 character mode
_video_text:
        ld hl,seq_text
        jr puts_loop

; void con_puts(const char *s)   fastcall: zero-terminated, sent unfiltered
_con_puts:
puts_loop:
        ld a,(hl)
        or a
        ret z
        call conout_a
        inc hl
        jr puts_loop

; void con_at(unsigned int row_col)   fastcall: L = row, H = column (zero based)
_con_at:
        push hl
        ld a,ESC
        call conout_a
        ld a,'Y'
        call conout_a
        pop hl
        ld a,l
        add a,32
        call conout_a
        ld a,h
        add a,32
        jr conout_a

; void conout(unsigned int c)   fastcall: L = byte
_conout:
        ld a,l
; Sends A through BIOS CONOUT, preserving HL, DE and BC.
conout_a:
        push hl
        push de
        push bc
        ld c,a
        ld hl,(BIOS)
        ld l,0x0c
        call jphl
        pop bc
        pop de
        pop hl
        ret

; unsigned char conready(void)   -- nonzero when BIOS CONST reports a key waiting
_conready:
        ld hl,(BIOS)
        ld l,0x06
        call jphl
        ld l,a
        ret

; unsigned char conin(void)   -- blocks in BIOS CONIN, returns the byte in L
_conin:
        ld hl,(BIOS)
        ld l,0x09
        call jphl
        ld l,a
        ret

jphl:   jp (hl)

SECTION rodata_user

seq_graphics:
        defb ESC,'3',ESC,'c',ESC,'Y',32,32,ESC,'k',0      ; ESC c hides the text cursor
seq_text:
        defb ESC,'4',ESC,'C',0                             ; ESC C shows it again

SECTION bss_user

_framebuffer:
        defs FB_SIZE
blit_width:
        defs 1
chunk_lines:
        defs 1
