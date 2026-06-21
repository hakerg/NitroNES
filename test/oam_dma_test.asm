; oam_dma_test.asm
; Minimal test: fills OAM page $02 and triggers OAM DMA.
; NESASM3 bank layout for .inesprg 2 (4 x 8KB = 32KB):
;   bank 0: $8000-$9FFF
;   bank 1: $A000-$BFFF
;   bank 2: $C000-$DFFF  <- code here
;   bank 3: $E000-$FFFF  <- vectors at $FFFA

	.inesprg 2
	.ineschr 0
	.inesmap 0
	.inesmir 1

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMDMA    = $4014

	.bank 2
	.org $C000

reset:
	sei
	cld
	ldx #$FF
	txs

	lda #$40
	sta $4017       ; disable APU frame counter IRQ

	lda #$00
	sta PPUCTRL
	sta PPUMASK

wait_vbl1:
	bit PPUSTATUS
	bpl wait_vbl1

wait_vbl2:
	bit PPUSTATUS
	bpl wait_vbl2

	ldx #$00
fill_oam:
	lda #$F0
	sta $0200,x
	inx
	bne fill_oam

	; write $02 to $4014 — trace trigger "oam" fires here
	lda #$02
	sta OAMDMA

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
