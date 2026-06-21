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

	; looping DMC sample, fastest rate
	lda #$4F
	sta $4010
	lda #$C0
	sta $4012
	lda #$01
	sta $4013

	lda #$10
	sta $4015       ; enable -> looping reload DMAs begin

	; let a few reload DMAs happen
	jsr wait200

	; disable in the middle - trace will show abort
	lda #$00
	sta $4015       ; <-- disable (abort)
	nop
	nop
	nop
	nop

forever:
	jmp forever

wait200:
	ldy #40
w1:
	dey
	bne w1
	rts

nmi_handler:
	rti
irq_handler:
	rti

	.bank 3
	.org $FFFA
	.word nmi_handler
	.word reset
	.word irq_handler

