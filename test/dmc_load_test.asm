	.inesprg 2
	.ineschr 0
	.inesmap 0
	.inesmir 1

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
	sta $2000
	sta $2001

	; set up a DMC sample
	lda #$4F        ; rate index 15 (fastest), loop
	sta $4010
	lda #$C0        ; sample address $C000... (= $C000 | (data<<6)) -> $C000
	sta $4012
	lda #$01        ; length
	sta $4013

	lda #$00
	sta $4015       ; make sure DMC off

	; enable DMC -> triggers a load DMA
	lda #$10
	sta $4015       ; <-- DMC enabled here
	lda $2002       ; read $2002 a few times while the load DMA runs
	lda $2002
	lda $2002

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

