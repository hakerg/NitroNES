; To assemble: snarfblasm.exe oam_flicker_test.asm
;
; Disables rendering at a controllable location in order to trigger OAM
; glitches. On certain PPUs, doing this during certain locations on a scanline
; can cause corruption of OAM for the next frame, even if OAM is written before
; then. See README.txt for full details.
;
; This issue is discussed here:
; https://forums.nesdev.com/viewtopic.php?f=3&t=19915
;
; Credit to blargg for the PPU/NMI synchronization code. See here for more
; information: http://forums.nesdev.com/viewtopic.php?t=6589

PPU_CONTROL := $2000
  kPpuControlScrollXHigh              := %00000001
  kPpuControlScrollYHigh              := %00000010
  kPpuControlVramIncrementRow         := %00000100
  kPpuControlSpriteAddress1000        := %00001000
  kPpuControlBgAddress1000            := %00010000
  kPpuControl8x16Sprites              := %00100000
  kPpuControlSlave                    := %01000000
  kPpuControlEnableNmi                := %10000000
PPU_MASK    := $2001
  kPpuMaskGreyscale                   := %00000001
  kPpuMaskEnableLeftmostColumnBg      := %00000010
  kPpuMaskEnableLeftmostColumnSprites := %00000100
  kPpuMaskEnableBg                    := %00001000
  kPpuMaskEnableSprites               := %00010000
  kPpuMaskEmphasizeRed                := %00100000
  kPpuMaskEmphasizeGreen              := %01000000
  kPpuMaskEmphasizeBlue               := %10000000
PPU_STATUS  := $2002
OAM_ADDRESS := $2003
OAM_DATA    := $2004
PPU_SCROLL  := $2005
PPU_ADDRESS := $2006
PPU_DATA    := $2007

DMC_FREQUENCY := $4010
DMC_RAW       := $4011
DMC_ADDRESS   := $4012
DMC_LENGTH    := $4013
OAM_DMA       := $4014
SOUND_CONTROL := $4015
JOYPAD1       := $4016
  kButtonRight  := $01
  kButtonLeft   := $02
  kButtonDown   := $04
  kButtonUp     := $08
  kButtonStart  := $10
  kButtonSelect := $20
  kButtonB      := $40
  kButtonA      := $80
JOYPAD2       := $4017
FRAME_COUNTER := $4017


split_mask_value      := $F0
refresh_oam           := $F1
set_oam_addr          := $F2
pulse_test            := $F3
joypad1_press         := $F4
joypad1_down          := $F5
joypad2_press         := $F6
joypad2_down          := $F7
scanline_delay        := $F8
cycle_delay           := $F9
frame                 := $FA

nmi_sync_count := $FF

sprite_region := $0200


.ORG $0000

    ; iNES Header.
    .db $4E,$45,$53,$1A,$01,$01,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00


.ORG  $0010
.BASE $C000

  Start:
    ; Disable everything.
    SEI
    LDA #0
    STA PPU_CONTROL
    STA PPU_MASK
    STA SOUND_CONTROL
    LDA #$40
    STA FRAME_COUNTER
    CLD

    ; Wait for the PPU to initialize.
    BIT PPU_STATUS
   -
    BIT PPU_STATUS
    BPL -
   -
    BIT PPU_STATUS
    BPL -

    JSR SetUpBackground
    JSR SetUpPalettes

    ; Set up DMC, which we (try) to use for NMI timing when OAM DMA is disabled.
    LDA #$0F
    STA DMC_FREQUENCY
    LDA #<(dDpcmAddress >> 6)
    STA DMC_ADDRESS
    LDA #$00
    STA DMC_RAW
    STA DMC_LENGTH

    ; Initialize our variables.
    LDA #$00
    STA joypad1_down
    STA joypad1_press
    STA pulse_test
    STA frame

    LDA #$01
    STA split_mask_value
    STA refresh_oam
    STA set_oam_addr

    LDA #$6E
    STA scanline_delay
    LDA #$80
    STA cycle_delay

    ; Synchronize to PPU and enable NMI.
    JSR InitNmiSync

    ; Spin.    
   -
    JSR WaitForNmi
    
    ; You could run normal code between NMIs here,
    ; as long as it completes BEFORE NMI. If it
    ; takes too long, synchronization may be off
    ; by a few cycles for that frame.
    ; ...
    
    JMP -


.dsb $D000 - $, $00

  ; All of the code up to a certain point has to be carefully timed. Except where specifically required for
  ; synchronization, everything must take a constant amount of time. This means we need to be careful about things like
  ; crossing pages and multi-path code.
  HandleNmi:
    ; We don't save X and Y because we don't need them outside the NMI and restoring them at the end of the NMI with
    ; our current timing can break the sync for the maximum scanline and cycle delay.
    ; 3 cycles.
    PHA

    ; This must be done before the DMA.
    ; 6 cycles (only counting the JSR in this one specific case; the interior can vary).
    JSR BeginNmiSync

    ; 5.
    INC frame

    ; 405.
    JSR HandleInput

    ; 164.
    JSR HandleBackground

    ; 6.
    LDA #$88
    STA PPU_CONTROL

    ; 10.
    LDA #$00
    STA PPU_SCROLL
    STA PPU_SCROLL

    ; If we're doing a test for the results of 1 frame of corruption, set the variables accordingly.
    ; 34 cycles.
    LDX pulse_test           ; 3
    BEQ +                    ; 3/2
    DEX                      ;   2
    STX pulse_test           ;   3
    LDA dPulseTestPpuMask,X  ;   4
    STA split_mask_value     ;   3
    LDA dPulseTestOamAddr,X  ;   4
    STA set_oam_addr         ;   3
    LDA dPulseTestOamDma,X   ;   4
    STA refresh_oam          ;   3
    JMP ++                   ;   3
   +
    LDY #5
   -
    DEY
    BNE -
    NOP
   ++

    ; Only set OAM address to 0 if requested. Turning this off means we retain the OAM address value from wherever we
    ; disabled rendering within sprite evaluation.
    ; 14.
    LDA set_oam_addr  ; 3
    BEQ +             ; 3/2
    LDA #$00          ;   2
    STA OAM_ADDRESS   ;   4
    BEQ ++            ;   3
   +
    NOP               ; 2
    NOP               ; 2
    NOP               ; 2
    NOP               ; 2
   ++    

    ; We use DMA to change the number of cycles we spend. If we're not using OAM DMA, we have to fall back on DMC DMA.
    ; 6/5.
    LDA refresh_oam
    BNE HandleNmi_OamDma

   HandleNmi_DmcDma:
    ; We're not doing OAM DMA on this path, so we have a lot of cycles we need to burn to make up for that.
    ; 501 cycles.
    LDY #50
   -
    NOP
    BIT $00
    DEY
    BNE -

    ; Do DMC DMA. We perform a write afterward so that if the DMA lands on it, it will be 1 cycle shorter. The hope is
    ; that this mimics the 1-cycle variance in OAM DMA length. This is compatible with some highly-accurate emulators,
    ; but in practice on real hardware, it seems to not work reliably. DMA timings are pretty well-understood, so I
    ; suspect the amount of time between enabling DMC and the first DMA may not be emulated correctly.
    ; 20 cycles.
    LDA #$10
    STA SOUND_CONTROL
    STA $00
    NOP
    NOP
    JMP HandleNmi_DelayLoop

   HandleNmi_OamDma:
    ; The number of cycles up to here must be even.
    ; 6.
    LDA #>dShadowOam
    STA $4014

   HandleNmi_DelayLoop:
    ; 6.
    LDA #$1E
    STA PPU_MASK

    ; Our instructions up to this point MUST total
    ; 1715 cycles, so we'll burn the rest in a loop.

    ; 1715 - 665 = 1050 cycles.
    LDY #104
   -
    NOP
    BIT $00
    DEY
    BNE -
    NOP
    NOP
    NOP
    BIT $00

    JSR EndNmiSync

    ; We're now synchronized exactly to 2286 cycles
    ; after beginning of frame.

    ; Delay for the specified number of scanlines.
    LDX scanline_delay
    BEQ HandleNmi_CycleDelay
    NOP
   --
    ; Spend 106 cycles.
    LDY #15
   -
    NOP
    DEY
    BNE -

    ; Another 8 cycles for all but the last loop.
    BIT $00
    DEX
    BNE --

   HandleNmi_CycleDelay:
    ; Do a really janky variable-cycle delay.
    LDA cycle_delay
    SEC
   -
    NOP
    SBC #$07
    BCS -

    ADC #$07

    ASL
    TAX
    LDA dDelayJumpTable,X
    STA $00
    LDA dDelayJumpTable+1,X
    STA $01
    JMP ($0000)


   HandleNmi_Delay5:
    JMP HandleNmi_Delay2
   HandleNmi_Delay3:
    JMP HandleNmi_Delay0
   HandleNmi_Delay1:
    JMP HandleNmi_DoSplit

   HandleNmi_Delay6:
    NOP
   HandleNmi_Delay4:
    NOP
   HandleNmi_Delay2:
    NOP
   HandleNmi_Delay0:
    NOP

   HandleNmi_DoSplit:
    ; Disable rendering (if we're currently configured to), which is the actual meat of the test.
    LDA split_mask_value
    STA PPU_MASK

    PLA
    RTI


  dDelayJumpTable:
    .dw HandleNmi_Delay0
    .dw HandleNmi_Delay1
    .dw HandleNmi_Delay2
    .dw HandleNmi_Delay3
    .dw HandleNmi_Delay4
    .dw HandleNmi_Delay5
    .dw HandleNmi_Delay6

  .IF $ > $D100
  .ERROR HandleNmi crosses a page boundary.
  .ENDIF


.dsb $D200 - $, $00
  ; Put this at the start of the page so we don't have to worry about branches crossing pages.
  HandleInput:  ; 393 + 12 (JSR/RTS).
    ; Strobe the joypad.
    ; 15 cycles.
    LDX #$01     ; 2
    STX $00      ; 3
    STX JOYPAD1  ; 4
    DEX          ; 2
    STX JOYPAD1  ; 4

    ; Read the buttons.
    ;; 16*8-1 = 127 cycles.
    ; 29*8-1 = 231 cycles.
   -
    LDA JOYPAD2  ; 4
    AND #$03     ; 2
    CMP #$01     ; 2
    ROL $01      ; 5

    LDA JOYPAD1  ; 4
    AND #$03     ; 2
    CMP #$01     ; 2
    ROL $00      ; 5
    BCC -        ; 3 / 2
    ; 246 cycles so far.

    ; Figure out which buttons were newly-pressed.
    ; 17 cycles.
    LDA $00           ; 3
    TAY               ; 2
    EOR joypad1_down  ; 3
    AND <$00          ; 3
    STA joypad1_press ; 3
    STY joypad1_down  ; 3
    ; 263 cycles so far.

    ; 17 cycles.
    LDA $01           ; 3
    TAY               ; 2
    EOR joypad2_down  ; 3
    AND <$01          ; 3
    STA joypad2_press ; 3
    STY joypad2_down  ; 3
    ; 280 cycles so far.

    ; If A is pressed on J2, start the multi-frame test.
    ; 14 cycles.
    LDA joypad2_press  ; 4
    BPL +              ; 3/2
    LDA #$04           ;   2
    STA pulse_test     ;   3
    JMP ++             ;   3
   +
    BIT $00            ; 3
    NOP                ; 2
    NOP                ; 2
   ++
    ; 294 cycles so far.

    ; If A is pressed, toggle the split mask value. We want to either have normal rendering (#$1E) or only
    ; greyscale (#$01).
    ; 16 cycles.
    LDA split_mask_value     ; 3
    BIT joypad1_press        ; 3
    BPL +                    ; 3/2
    EOR #$1F                 ;   2
    BPL ++  ; Unconditional. ;   3
   +
    NOP                      ; 2
    NOP                      ; 2
   ++
    STA split_mask_value     ; 3
    ; 310 cycles so far.

    ; If select is pressed, toggle OAM address writes.
    ; 18 cycles.
    LDA joypad1_press   ; 3
    AND #kButtonSelect  ; 2
    BEQ +               ; 3/2
    LDA set_oam_addr    ;   3
    EOR #$01            ;   2
    STA set_oam_addr    ;   3
    JMP ++              ;   3
   +
    NOP
    NOP
    NOP
    NOP
    NOP
   ++
    ; 328 cycles so far.

    ; If start is pressed, toggle OAM DMA.
    ; 18 cycles.
    LDA joypad1_press   ; 3
    AND #kButtonStart   ; 2
    BEQ +               ; 3/2
    LDA refresh_oam     ;   3
    EOR #$01            ;   2
    STA refresh_oam     ;   3
    JMP ++              ;   3
   +
    NOP
    NOP
    NOP
    NOP
    NOP
   ++
    ; 346 cycles so far.

    ; If B is held, use down instead of press so delay updates are applied every frame.
    ; 17 cycles.
    LDA joypad1_down     ; 3
    ROL                  ; 2
    ROL                  ; 2
    ROL                  ; 2
    AND #$01             ; 2
    TAX                  ; 2
    LDA joypad1_press,X  ; 4
    ; 363 cycles so far.

    ; Handle the directions.
    LSR
    BCS HandleInput_ButtonRight   ; 5
    LSR
    BCS HandleInput_ButtonLeft    ; 9
    LSR
    BCS HandleInput_ButtonDown    ; 13
    LSR
    BCC ClockSlide-13             ; 17
    
   HandleInput_ButtonUp:         ; 16
    LDX scanline_delay    ; 3
    BEQ +                 ; 3/2
    DEX                   ;   2
    STX <>scanline_delay  ;   4
    JMP ClockSlide_None   ;   3. 14, +16 = 30 cycles total.
   +
    LDX #$EE              ; 2
    STX scanline_delay    ; 3
    JMP ClockSlide_None   ; 3. 14, +16 = 30 cycles total.


   HandleInput_ButtonDown:       ; 13
    LDX scanline_delay    ; 3
    CPX #$EE              ; 2
    BCS +                 ; 3/2
    INX                   ;   2  
    STX scanline_delay    ;   3
    JMP ClockSlide-2      ;   3. 15, +13 = 28 cycles total.
   +
    LDX #$00              ; 2
    STX <>scanline_delay  ; 4
    JMP ClockSlide_None   ; 3. 17, +13 = 30 cycles total.


   HandleInput_ButtonLeft:       ; 9
    DEC cycle_delay       ; 5
    JMP ClockSlide-13     ; 3. 8, +9 = 17 cycles total.


   HandleInput_ButtonRight:       ; 5
    INC cycle_delay       ; 5
    JMP ClockSlide-17     ; 3. 8, +5 = 13 cycles total.


    ; The location we jump into here determines how many cycles we burn. We burn 1 more cycle than the number of bytes
    ; executed. The labels are set up so that we jump to ClockSlide-n to burn n cycles, where n cannot be 1.
    .db $C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9,$C9
    .db $C9,$C9,$C5,$EA
  ClockSlide_None:
    RTS
  ClockSlide:
    ; Put an RTS here to be safe, in case we jump to ClockSlide-0.
    RTS


  .IF $ > $D300
  .ERROR HandleInput crosses a page boundary.
  .ENDIF


.dsb $D300 - $, $00
  ; Put this at the start of the page so we don't have to worry about branches crossing pages.
  HandleBackground:  ; 152 + 12 (JSR/RTS).
    ; Draw OAM address state.
    ; 12.
    LDA #$20
    STA PPU_ADDRESS
    LDA #$DC
    STA PPU_ADDRESS

    ; 9.
    LDA frame
    AND #$01
    STA PPU_DATA
    ; 21 so far.

    ; Draw scanline delay state.
    ; 12.
    LDA #$20
    STA PPU_ADDRESS
    LDA #$65
    STA PPU_ADDRESS
    ; 33 so far.

    ; 15.
    LDA scanline_delay
    LSR
    LSR
    LSR
    LSR
    STA PPU_DATA
    ; 48 so far.

    ; 9.
    LDA scanline_delay
    AND #$0F    
    STA PPU_DATA
    ; 57 so far.

    ; Draw cycle delay state.
    ; 12.
    LDA #$20
    STA PPU_ADDRESS
    LDA #$85
    STA PPU_ADDRESS
    ; 69 so far.

    ; 15.
    LDA cycle_delay
    LSR
    LSR
    LSR
    LSR
    STA PPU_DATA
    ; 84 so far.

    ; 9.
    LDA cycle_delay
    AND #$0F    
    STA PPU_DATA
    ; 93 so far.

    ; Draw OAM DMA state.
    ; 12.
    LDA #$20
    STA PPU_ADDRESS
    LDA #$9D
    STA PPU_ADDRESS
    ; 105 so far.

    ; 7.
    LDA refresh_oam
    STA PPU_DATA
    ; 112 so far.

    ; Draw OAM address state.
    ; 12.
    LDA #$20
    STA PPU_ADDRESS
    LDA #$7D
    STA PPU_ADDRESS
    ; 124 so far.

    ; 7.
    LDA set_oam_addr
    STA PPU_DATA
    ; 131 so far.

    ; Draw OAM address state.
    ; 12.
    LDA #$20
    STA PPU_ADDRESS
    LDA #$A9
    STA PPU_ADDRESS
    ; 143 so far.

    ; 9.
    LDA split_mask_value
    AND #$01
    STA PPU_DATA
    ; 152 so far.

    RTS


  SetUpBackground:
    ; Clear nametable 0.
    LDA PPU_STATUS
    LDA #$20
    STA PPU_ADDRESS
    LDA #$00
    STA PPU_ADDRESS

    LDA #$FF
    LDY #$00
   -
    STA PPU_DATA
    STA PPU_DATA
    STA PPU_DATA
    STA PPU_DATA
    INY
    BNE -

    ; Write our text to the screen.
    LDX #$00
   SetUpBackground_SequenceLoop:
    ; If the byte is negative, the data stream is finished.
    LDA dPpuSequence,X
    BMI SetUpBackground_Done

    ; Write the address.
    STA PPU_ADDRESS
    INX

    LDA dPpuSequence,X
    STA PPU_ADDRESS
    INX

    ; Get the length.
    LDA dPpuSequence,X
    TAY
    INX

    ; Write the data.
   -
    LDA dPpuSequence,X
    STA PPU_DATA
    INX

    DEY
    BNE -
    BEQ SetUpBackground_SequenceLoop

   SetUpBackground_Done:
    RTS


  ; VRAM write strings. Format is PPU address, length, data. Struct is $FF terminated and cannot be more than 256 bytes
  ; long.
  dPpuSequence:
    .db $20,$6B, $01, $50  ; 0
    .db $20,$4E, $03, $53,$52,$55  ; +8->
    .db $20,$E9, $02, $53,$51  ; +1
    .db $21,$0A, $01, $54  ; v

    .db $20,$42, $09, $20,$35,$2C,$37,$28,$FF,$19,$19,$1E  ; Write PPU
    .db $20,$62, $02, $1E,$0D  ; UD
    .db $20,$82, $02, $15,$1B  ; LR
    .db $20,$A2, $06, $0A,$FF,$02,$00,$00,$01  ; A 2001
    .db $20,$C2, $06, $0B,$FF,$0F,$24,$36,$37  ; B Fast

    .db $20,$55, $09, $20,$35,$2C,$37,$28,$FF,$18,$0A,$16  ; Write OAM
    .db $20,$75, $07, $1C,$28,$FF,$02,$00,$00,$03  ; Se 2003
    .db $20,$95, $07, $1C,$37,$FF,$04,$00,$01,$04  ; St 4014

    .db $20,$D5, $06, $19,$24,$35,$2C,$37,$3C  ; Parity

    .db $FF  ; (Terminate)
   dPpuSequence_End:

  .IF dPpuSequence_End - dPpuSequence > 256
  .ERROR dPpuSequence is too long.
  .ENDIF


  SetUpPalettes:
    LDA PPU_STATUS
    LDA #$3F
    STA PPU_ADDRESS
    LDA #$00
    STA PPU_ADDRESS

    LDX #$20
    LDY #$00
   -
    LDA dPaletteData,Y
    STA PPU_DATA
    INY
    DEX
    BNE -

    RTS

  dPaletteData:
    .db $0F,$30,$36,$0B,$0F,$30,$36,$0B,$0F,$30,$36,$0B,$0F,$30,$36,$0B
    .db $0F,$21,$11,$30,$0F,$15,$05,$30,$0F,$30,$10,$00,$0F,$00,$10,$30


.dsb $D800 - $, $00

  dShadowOam:
    .db $1F,$80,$00,$60
    .db $27,$80,$01,$60
    .db $2F,$80,$00,$60
    .db $37,$80,$01,$60
    .db $3F,$80,$00,$60
    .db $47,$80,$01,$60
    .db $4F,$80,$00,$60
    .db $57,$80,$01,$60

    .db $1E,$80,$01,$68
    .db $26,$80,$00,$68
    .db $2E,$80,$01,$68
    .db $36,$80,$00,$68
    .db $3E,$80,$01,$68
    .db $46,$80,$00,$68
    .db $4E,$80,$01,$68
    .db $56,$80,$00,$68

    .db $1D,$80,$00,$70
    .db $25,$80,$01,$70
    .db $2D,$80,$00,$70
    .db $35,$80,$01,$70
    .db $3D,$80,$00,$70
    .db $45,$80,$01,$70
    .db $4D,$80,$00,$70
    .db $55,$80,$01,$70

    .db $1C,$80,$01,$78
    .db $24,$80,$00,$78
    .db $2C,$80,$01,$78
    .db $34,$80,$00,$78
    .db $3C,$80,$01,$78
    .db $44,$80,$00,$78
    .db $4C,$80,$01,$78
    .db $54,$80,$00,$78

    .db $1B,$80,$00,$80
    .db $23,$80,$01,$80
    .db $2B,$80,$00,$80
    .db $33,$80,$01,$80
    .db $3B,$80,$00,$80
    .db $43,$80,$01,$80
    .db $4B,$80,$00,$80
    .db $53,$80,$01,$80

    .db $1A,$80,$01,$88
    .db $22,$80,$00,$88
    .db $2A,$80,$01,$88
    .db $32,$80,$00,$88
    .db $3A,$80,$01,$88
    .db $42,$80,$00,$88
    .db $4A,$80,$01,$88
    .db $52,$80,$00,$88

    .db $19,$80,$00,$90
    .db $21,$80,$01,$90
    .db $29,$80,$00,$90
    .db $31,$80,$01,$90
    .db $39,$80,$00,$90
    .db $41,$80,$01,$90
    .db $49,$80,$00,$90
    .db $51,$80,$01,$90

    .db $18,$80,$01,$98
    .db $20,$80,$00,$98
    .db $28,$80,$01,$98
    .db $30,$80,$00,$98
    .db $38,$80,$01,$98
    .db $40,$80,$00,$98
    .db $48,$80,$01,$98
    .db $50,$80,$00,$98


.dsb $E000 - $, $00
  ; blargg's synchronization code.

  ; Initializes synchronization and enables NMI
  ; Preserved: X, Y
  ; Time: 15 frames average, 28 frames max
  InitNmiSync:
    ; Disable interrupts and rendering
    SEI
    LDA #0
    STA PPU_CONTROL
    STA PPU_MASK

    ; Coarse synchronize
    BIT PPU_STATUS
   InitNmiSync_1:
    BIT PPU_STATUS
    BPL InitNmiSync_1

    ; Synchronize to odd CPU cycle
    STA OAM_DMA

    ; Fine synchronize
    LDA #3
   InitNmiSync_2:
    STA nmi_sync_count
    BIT PPU_STATUS
    BIT PPU_STATUS
    PHP
    EOR #$02
    NOP
    NOP
    PLP
    BPL InitNmiSync_2

    ; Delay one frame
   InitNmiSync_3:
    BIT PPU_STATUS
    BPL InitNmiSync_3

    ; Enable rendering long enough for frame to
    ; be shortened if it's a short one, but not long
    ; enough that background will get displayed.
    LDA #$08
    STA PPU_MASK

    ; Can reduce delay by up to 5 and this still works,
    ; so there's a good margin.
    ; delay 2377
    LDA #216
   InitNmiSync_4:
    NOP
    NOP
    SEC
    SBC #1
    BNE InitNmiSync_4

    STA $2001

    LDA nmi_sync_count

    ; Wait for this and next frame to finish.
    ; If this frame was short, loop ends. If it was
    ; long, loop runs for a third frame.
   InitNmiSync_5:
    BIT PPU_STATUS
    BIT PPU_STATUS
    PHP
    EOR #$02
    STA nmi_sync_count
    NOP
    NOP
    PLP
    BPL InitNmiSync_5

    ; Enable NMI
    LDA #$80
    STA PPU_CONTROL

    RTS


  ; Waits until NMI occurs.
  ; Preserved: A, X, Y
  WaitForNmi:
    PHA
	
    ; Reset high/low flag so NMI can depend on it
    BIT PPU_STATUS
	
    ; NMI must not occur during taken branch, so we
    ; only use branch to get out of loop.
    LDA nmi_sync_count
   WaitNmi_1:
    CMP nmi_sync_count
    BNE WaitNmi_2
    JMP WaitNmi_1
   WaitNmi_2:
    PLA
    RTS


  ; Must be called in NMI handler, before sprite DMA.
  ; Preserved: X, Y
  BeginNmiSync:
    LDA nmi_sync_count
    AND #$02
    BEQ BeingNmiSync_1
   BeingNmiSync_1:
    RTS


  ; Must be called after sprite DMA. Instructions before this
  ; must total 1715 (NTSC)/6900 (PAL) cycles, treating
  ; JSR BeingNmiSync and STA $4014 as taking 10 cycles total) 
  ; Next instruction will begin 2286 (NTSC)/7471 (PAL) cycles
  ; after the cycle that the frame began in.
  ; Preserved: X, Y
  EndNmiSync:
    LDA nmi_sync_count
    INC nmi_sync_count
    AND #$02
    BNE EndNmiSync_1
   EndNmiSync_1:
    LDA PPU_STATUS
    BMI EndNmiSync_2
   EndNmiSync_2:
    BMI EndNmiSync_3
   EndNmiSync_3:
    RTS


  ; Keeps track of synchronization on frames where no
  ; synchronization is needed (where BeingNmiSync/EndNmiSync
  ; aren't called).
  ; Preserved: A, X, Y
  TrackNmiSync:
    INC nmi_sync_count
    RTS

  .IF $ > $E100
  .ERROR NmiSync routines cross a page boundary.
  .ENDIF


.dsb $FFC0 - $, $00

  ; Some DPCM data so the DMC DMA doesn't play any sound.
  dDpcmAddress:
  .db $00

  ; Per-frame values when handling the pulse test. These are used in reverse order and the final entry sticks.
  dPulseTestPpuMask:
    .db $1E,$1E,$01,$1E
  dPulseTestOamAddr:
    .db $01,$01,$01,$01
  dPulseTestOamDma:
    .db $00,$01,$01,$01


.dsb $FFFA - $, $00

  .dw HandleNmi
  .dw Start
  .dw Start

  .INCBIN "chr.dat"