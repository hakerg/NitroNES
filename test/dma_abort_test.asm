        .inesprg 2
        .ineschr 0
        .inesmap 0
        .inesmir 1

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
RESULT    = $00

        .bank 2
        .org $C000

reset:
        sei
        cld
        ldx #$FF
        txs
        lda #$40
        sta $4017

        lda #$00
        sta PPUCTRL
        sta PPUMASK

wait_vbl1:
        bit PPUSTATUS
        bpl wait_vbl1
wait_vbl2:
        bit PPUSTATUS
        bpl wait_vbl2

        lda #$01
        sta RESULT

        ; Set up a looping DMC sample at $C100
        lda #$4F        ; loop, max speed
        sta $4010
        lda #$00
        sta $4011
        lda #$C1        ; sample address $C100
        sta $4012
        lda #$10        ; length = 17 bytes
        sta $4013

        ; Start DMC
        lda #$10
        sta $4015

        ; Wait for DMA to sync (wait for a byte to be fetched)
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop

        ; Now disable DMC - this should trigger explicit abort if reload DMA was about to happen
        lda #$00
        sta $4015

forever:
        jmp forever

nmi_handler:
        rti

irq_handler:
        rti

        .bank 3
        .org $FFFA
        .word nmi_handler
        .word reset
        .word irq_handler
