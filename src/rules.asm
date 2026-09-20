; rules.asm -- the hot paths of the Othello rules in Z80 assembly.
;
; The computer's search calls these thousands of times per move. A single
; direction walk serves three entry points: total flip count, legality with
; an early exit at the first capture, and playing a move with in-place
; flipping. The board is 64 bytes (row-major, A1 = 0); board_steps, built
; by board_init() in C, gives the squares available in each direction so
; the walk needs no edge checks. No IX/IY, no alternate registers (the BIOS
; interrupt handler may use them), so all state lives in registers and a
; few scratch bytes.
;
; Calling convention (Z88DK, sdcc_iy): __z88dk_fastcall, argument in HL
; (L = cell, H = colour), 8-bit result in L, 16-bit result in HL.

SECTION code_user

PUBLIC _board_flips_fc
PUBLIC _board_legal_fc
PUBLIC _board_play_fc
PUBLIC _board_evaluate

EXTERN _board
EXTERN _board_steps

defc MODE_TOTAL = 0
defc MODE_LEGAL = 1
defc MODE_PLAY  = 2

; unsigned char board_flips_fc(unsigned int cell_me): opponent discs the move flips
_board_flips_fc:
        ld a,MODE_TOTAL
        jr scan

; unsigned char board_legal_fc(unsigned int cell_me): nonzero when the move captures
_board_legal_fc:
        ld a,MODE_LEGAL
        jr scan

; void board_play_fc(unsigned int cell_me): place the disc and flip what it brackets
_board_play_fc:
        ld a,MODE_PLAY

scan:
        ld (scan_mode),a
        ld a,h
        ld (scan_me),a
        xor a
        ld (scan_total),a
        ld h,a                  ; HL = cell
        push hl
        add hl,hl               ; scan_row = &board_steps[cell][0]
        add hl,hl
        add hl,hl
        ld de,_board_steps
        add hl,de
        ld (scan_row),hl
        pop hl
        ld de,_board            ; scan_ptr = &board[cell]
        add hl,de
        ld (scan_ptr),hl
        ld a,(scan_mode)
        cp MODE_PLAY
        jr z,scan_place
        ld a,(hl)
        or a
        jr nz,scan_zero         ; occupied square: no move
        jr scan_start
scan_place:
        ld a,(scan_me)
        ld (hl),a
scan_start:
        ld d,0                  ; D = direction index 0..7
dir_loop:
        ld hl,(scan_row)        ; A = board_steps[cell][d]
        ld a,l
        add a,d
        ld l,a
        jr nc,no_carry
        inc h
no_carry:
        ld a,(hl)
        or a
        jr z,next_dir
        ld (scan_n0),a
        push de                 ; keep the direction index
        ld hl,steps16           ; DE = sign-extended step
        ld e,d
        ld d,0
        add hl,de
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld hl,(scan_ptr)        ; HL = &board[cell]
        ld a,(scan_n0)          ; B = squares available, C = me
        ld b,a
        ld a,(scan_me)
        ld c,a
walk:                           ; B squares left, C = me, DE = step
        add hl,de
        ld a,(hl)
        or a
        jr z,walk_none          ; empty: nothing bracketed
        cp c
        jr z,walk_done          ; own disc closes the bracket
        djnz walk
walk_none:
        pop de
        jr next_dir
walk_done:
        ld a,(scan_n0)
        sub b                   ; captures = squares walked before the own disc
        jr z,walk_empty         ; adjacent own disc: nothing captured
        ld b,a
        ld a,(scan_total)
        add a,b
        ld (scan_total),a
        ld a,(scan_mode)
        cp MODE_PLAY
        jr nz,walk_scored
flip_back:                      ; walk back over the captures, recolouring them
        or a
        sbc hl,de
        ld (hl),c
        djnz flip_back
        jr walk_empty
walk_scored:
        cp MODE_LEGAL
        jr nz,walk_empty
        pop de
        jr scan_end             ; legal: one capture is enough
walk_empty:
        pop de
next_dir:
        inc d
        ld a,d
        cp 8
        jr c,dir_loop
scan_end:
        ld a,(scan_total)
        ld l,a
        ret
scan_zero:
        ld l,0
        ret

; int board_evaluate(unsigned int me): positional score from me's point of view.
; Sum of the weight of every own disc minus that of every opponent disc.
_board_evaluate:
        ld a,l
        ld (scan_me),a
        ld bc,0                 ; C = square index
        ld de,0                 ; DE = sum
eval_loop:
        ld hl,_board
        add hl,bc
        ld a,(hl)
        or a
        jr z,eval_next
        ld hl,scan_me
        cp (hl)
        ld hl,weights
        add hl,bc
        ld a,(hl)               ; weight, flags still from the comparison
        jr z,eval_add
        neg
eval_add:
        ld l,a                  ; HL = sign-extended weight
        rla
        sbc a,a
        ld h,a
        add hl,de
        ex de,hl
eval_next:
        inc c
        ld a,c
        cp 64
        jr nz,eval_loop
        ex de,hl
        ret

SECTION rodata_user

steps16: defw -9,-8,-7,-1,1,7,8,9

; Positional weights for the search: corners are permanent, the squares
; next to them hand the corner to the opponent, edges are worth a little.
weights:
        defb 100,-20, 10,  5,  5, 10,-20,100
        defb -20,-50, -2, -2, -2, -2,-50,-20
        defb  10, -2, -1, -1, -1, -1, -2, 10
        defb   5, -2, -1, -1, -1, -1, -2,  5
        defb   5, -2, -1, -1, -1, -1, -2,  5
        defb  10, -2, -1, -1, -1, -1, -2, 10
        defb -20,-50, -2, -2, -2, -2,-50,-20
        defb 100,-20, 10,  5,  5, 10,-20,100

SECTION bss_user

scan_mode:  defs 1
scan_me:    defs 1
scan_n0:    defs 1
scan_total: defs 1
scan_ptr:   defs 2
scan_row:   defs 2
