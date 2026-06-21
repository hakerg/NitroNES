; nes_template.asm — minimal NROM-256 test ROM for asm_run
;
; *** NESASM3 bank layout (IMPORTANT) ***
; .inesprg 2 = 32KB PRG = 4 x 8KB banks:
;   bank 0: $8000-$9FFF  (empty, leave as-is)
;   bank 1: $A000-$BFFF  (empty, leave as-is)
;   bank 2: $C000-$DFFF  <- put your test code here
;   bank 3: $E000-$FFFF  <- vectors MUST be here at $FFFA
;
; HOW TO USE:
;   1. Copy this file, rename it (e.g. mytest.asm)
;   2. Write your test code in the "TEST CODE BEGIN/END" section
;   3. Run:   asm_run mytest.asm
;      Trace: asm_run --trigger oam --pre 50 --post 520 mytest.asm
;             asm_run --trigger nmi --pre 50 --post 50 mytest.asm
;             asm_run --trigger addr:XXXX mytest.asm
;
; Result convention: write a byte to RAM address $00:
;   $01 = PASS
;   $02+ = FAIL (error code)

	.inesprg 2
	.ineschr 0
	.inesmap 0
	.inesmir 1

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMDMA    = $4014
RESULT    = $00

	.bank 2
	.org $C000

reset:
	sei
	cld
	ldx #$FF
	txs
	lda #$40
	sta $4017          ; disable APU frame counter IRQ

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
	sta RESULT         ; default: PASS

	; --- TEST CODE BEGIN ---

	; write your test here
	; on fail: lda #$02 : sta RESULT

	; --- TEST CODE END ---

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
