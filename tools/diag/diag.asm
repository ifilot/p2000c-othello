; DIAG.COM -- terminal diagnostics for the P2000C, to be run on real hardware.
;
; Six tests, each announced on a cleared text screen; press a key to run it
; and a key to continue. Photograph the result of every test.
;   T1  24 text lines of 79 chars, BIOS CONOUT at full speed (like the help page)
;   T2  the same lines through BDOS function 6 (direct console output)
;   T3  the same lines through BIOS CONOUT with ~2 ms between bytes
;   T4  graphics mode: a box drawn with terminal vector commands + a text label
;   T5  graphics mode: three ESC r picture uploads at known coordinates
;   T6  ESC r of 2 lines at the bottom line: a solid line, then a dotted one.
;       The picture RAM runs bottom-up, so the dotted line appears ABOVE it.
;   T7  ESC r of an 11-line staircase starting 11 lines above the bottom:
;       the steps climb up and to the right.
; Rules established on hardware: a count with a zero low byte fails, and the
; bytes of an upload fill a line and then continue on the line above it.
; Assemble: z80asm -o DIAG.COM diag.asm

FB:     equ 4000h
ESC:    equ 27
BIOS:   equ 0001h

        org 0100h
        ld (ccp_sp),sp
        ld sp,stack_top

; ---- T1: BIOS, full speed
        ld hl,banner1
        call banner
        ld a,0
        ld (mode),a
        call text_lines
        call conin

; ---- T2: BDOS 6
        ld hl,banner2
        call banner
        ld a,1
        ld (mode),a
        call text_lines
        call conin

; ---- T3: BIOS, throttled
        ld hl,banner3
        call banner
        ld a,2
        ld (mode),a
        call text_lines
        ld a,0
        ld (mode),a
        call conin

; ---- T4: vectors
        ld hl,banner4
        call banner
        ld hl,t4_seq
        ld bc,t4_len
        call sendn
        call conin
        ld hl,seq_text
        call puts

; ---- T5: ESC r placements
        ld hl,banner5
        call banner
        ld hl,t5_head
        ld bc,t5_head_len
        call sendn
        ; (0,251) 64 bytes of FFh: emulator model = top line
        ld hl,t5_a
        ld bc,7
        call sendn
        ld a,0ffh
        ld bc,64
        call fill_send
        ; (0,0) 64 bytes of AAh: emulator model = bottom line, dotted
        ld hl,t5_b
        ld bc,7
        call sendn
        ld a,0aah
        ld bc,64
        call fill_send
        ; (0,200) 640 bytes: 5 x (64 x FFh, 64 x 00h): stripes on alternate lines
        ld hl,t5_c
        ld bc,7
        call sendn
        ld b,5
t5loop: push bc
        ld a,0ffh
        ld bc,64
        call fill_send
        xor a
        ld bc,64
        call fill_send
        pop bc
        djnz t5loop
        call conin
        ld hl,seq_text
        call puts

; ---- T6/T7: upload direction.
        ld hl,banner6
        call banner
        ld hl,t6_head
        ld bc,t6_head_len
        call sendn
        ld a,0ffh
        ld bc,64
        call fill_send          ; first line: solid, lands on the bottom line
        ld a,0aah
        ld bc,64
        call fill_send          ; second line: dotted, lands on the line above
        call conin
        ld hl,seq_text
        call puts

        ld hl,banner7
        call banner
        call build_stairs
        ld hl,t7_head
        ld bc,t7_head_len
        call sendn
        ld hl,FB
        ld bc,704
        call sendn              ; 11 staircase lines, drawn upward from y = 10
        call conin
        ld hl,seq_text
        call puts

        ld sp,(ccp_sp)
        ret

; 63 lines: line k has byte k = 0FFh (a bar at x = 8k..8k+7)
build_stairs:
        ld hl,FB
        ld de,FB+1
        ld bc,63*64-1
        ld (hl),0
        ldir
        ld hl,FB
        ld de,65                ; next line, next byte
        ld b,63
bs1:    ld (hl),0ffh
        add hl,de
        djnz bs1
        ret

; --- helpers ------------------------------------------------------------------

; clear the text screen and print a banner (HL), then wait for a key
banner: push hl
        ld hl,seq_clear
        call puts
        pop hl
        call puts
        ld hl,msg_key
        call puts
        call conin
        ld hl,seq_clear
        jp puts

; 24 rows: ESC Y row col0, "Rnn " + digits to 79 columns
text_lines:
        ld b,0
tl_row: push bc
        ld a,ESC
        call out
        ld a,'Y'
        call out
        ld a,b
        add a,32
        call out
        ld a,32
        call out
        ld a,'R'
        call out
        ld a,b
        call put_dec
        ld a,' '
        call out
        ld c,75                 ; 4 + 75 = 79 columns
        ld a,'0'
tl_chr: call out
        inc a
        cp '9'+1
        jr nz,tl_next
        ld a,'0'
tl_next:
        dec c
        jr nz,tl_chr
        pop bc
        inc b
        ld a,b
        cp 24
        jr nz,tl_row
        ret

put_dec:                        ; A = 0..99 as two digits
        ld c,'0'
pd10:   cp 10
        jr c,pd1
        sub 10
        inc c
        jr pd10
pd1:    push af
        ld a,c
        call out
        pop af
        add a,'0'
        jp out

; output A according to mode: 0 BIOS, 1 BDOS 6, 2 BIOS + delay; preserves all
out:    push af
        push hl
        push de
        push bc
        ld c,a
        ld a,(mode)
        cp 1
        jr z,out_bdos
        cp 2
        jr z,out_slow
        ld a,c
        call conout_a
        jr out_done
out_bdos:
        ld e,c
        ld c,6
        call 5
        jr out_done
out_slow:
        ld a,c
        call conout_a
        ld b,0
slow:   nop
        nop
        nop
        djnz slow               ; 256 x ~30 T = ~2 ms at 4 MHz
out_done:
        pop bc
        pop de
        pop hl
        pop af
        ret

; send BC copies of A through BIOS CONOUT
fill_send:
        ld d,a
fs_loop:
        ld a,d
        call conout_a
        dec bc
        ld a,b
        or c
        jr nz,fs_loop
        ret

; send BC bytes from HL through BIOS CONOUT
sendn:  ld a,(hl)
        call conout_a
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,sendn
        ret

puts:   ld a,(hl)
        or a
        ret z
        call conout_a
        inc hl
        jr puts

conout_a:
        push hl
        push de
        push bc
        ld c,a
        ld hl,(BIOS)
        ld l,0ch
        call jphl
        pop bc
        pop de
        pop hl
        ret
conin:  ld hl,(BIOS)
        ld l,09h
jphl:   jp (hl)

; --- data ----------------------------------------------------------------------

mode:   db 0
seq_clear: db 12,ESC,'c',0
seq_text:  db ESC,'4',0
msg_key:   db 13,10,13,10,'Druk op een toets.',0

banner1: db 'T1  24 regels van 79 tekens via BIOS CONOUT, volle snelheid',13,10
         db '    Verwacht: R00..R23 netjes onder elkaar, cijfers 0-9 herhaald.',0
banner2: db 'T2  Dezelfde regels via BDOS functie 6',0
banner3: db 'T3  Dezelfde regels via BIOS CONOUT met 2 ms pauze per teken',0
banner4: db 'T4  Grafische stand (ESC 3): rechthoek met vectorcommandos + tekst',13,10
         db '    Verwacht: rechthoek (10,10)-(500,240), stip in het midden, tekst bovenin',0
banner5: db 'T5  ESC r op drie plaatsen',13,10
         db '    Verwacht: dichte lijn BOVENAAN, stippellijn ONDERAAN,',13,10
         db '    5 dunne strepen 51 lijnen onder de bovenrand',0
banner6: db 'T6  ESC r van 2 lijnen op de onderste lijn: eerst een dichte, dan een stippellijn',13,10
         db '    Verwacht: dichte lijn ONDERAAN met de stippellijn er direct BOVEN.',0
banner7: db 'T7  ESC r van een trap van 11 lijnen, beginnend 11 lijnen boven de onderrand',13,10
         db '    Verwacht: de trap klimt naar rechtsBOVEN, van links onder naar rechts.',0

t4_seq: db ESC,'3',ESC,'Y',32,32,'T4 vectoren: kader (10,10)-(500,240)'
        db ESC,'m',10,0,10, ESC,'M',244,1,10, ESC,'M',244,1,240, ESC,'M',10,0,240, ESC,'M',10,0,10
        db ESC,'D',250,0,125
t4_len: equ $-t4_seq
t5_head: db ESC,'3',ESC,'Y',32,32,'T5 ESC r: boven vol, onder stippel, strepen'
t5_head_len: equ $-t5_head
t5_a:   db ESC,'r',0,0,251,64,0
t5_b:   db ESC,'r',0,0,0,64,0
t5_c:   db ESC,'r',0,0,200,128,2          ; 640 = 0x0280
t6_head: db ESC,'3',ESC,'Y',32+1,32,'T6 dicht onderaan, stippel erboven',ESC,'r',0,0,0,080h,000h
t6_head_len: equ $-t6_head
t7_head: db ESC,'3',ESC,'Y',32+1,32,'T7 trap klimt naar rechtsboven',ESC,'r',0,0,10,0c0h,002h
t7_head_len: equ $-t7_head

ccp_sp: dw 0
        ds 128
stack_top:
