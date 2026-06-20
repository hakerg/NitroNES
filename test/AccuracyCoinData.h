#pragma once
// AccuracyCoin test descriptions transcribed from
// nes-test-roms-master/AccuracyCoin-main/README.md
// Codes: red codes = failure error codes, blue codes = acceptable-variant "success" codes.

struct AcCode { char code; const char* text; };
struct AcTest { const char* name; const AcCode* err; int errN; const AcCode* ok; int okN; };
struct AcPage { const char* name; const AcTest* tests; int testN; };

#define AC_TST(name, err)        { name, err, (int)(sizeof(err)/sizeof((err)[0])), nullptr, 0 }
#define AC_TSTV(name, err, ok)   { name, err, (int)(sizeof(err)/sizeof((err)[0])), ok, (int)(sizeof(ok)/sizeof((ok)[0])) }
#define AC_DRAW(name)            { name, nullptr, 0, nullptr, 0 }

// ===================== Page 1: CPU Behavior =====================
static const AcCode e_ROMNotWritable[] = {
    {'1',"Writing to ROM should not overwrite the byte in ROM."},
};
static const AcCode e_RAMMirror[] = {
    {'1',"Reading from a 13-bit mirror of an address in RAM should have the same value as the 11-bit address."},
    {'2',"Writing to a 13-bit mirror of an address in RAM should write to the 11-bit address."},
};
static const AcCode e_PCWrap[] = {
    {'1',"Executing address $FFFF should read addresses $0000 and $0001 as the operands."},
};
static const AcCode e_DecimalFlag[] = {
    {'1',"The 6502 BCD flag should not affect ADC/SBC on the NES."},
    {'2',"Despite this flag not working, it still gets pushed in a PHP/BRK instruction."},
};
static const AcCode e_BFlag[] = {
    {'1',"The B flag should be set by PHP."},
    {'2',"The B flag should be set by BRK."},
    {'3',"An IRQ should have occured."},
    {'4',"The B flag should not be set by an IRQ."},
    {'5',"The B flag should not be set by an NMI."},
    {'6',"Bit 5 of the processor flags should be set by PHP."},
    {'7',"Bit 5 of the processor flags should be set by BRK."},
    {'8',"Bit 5 of the processor flags should be set by an IRQ."},
    {'9',"Bit 5 of the processor flags should be set by an NMI."},
};
static const AcCode e_DummyReads[] = {
    {'1',"A mirror of PPU_STATUS ($2002) should be read twice by LDA $20F2,X (X=$10)."},
    {'2',"The dummy read should not occur if a page boundary is not crossed."},
    {'3',"The dummy read was on an incorrect address."},
    {'4',"The STA,X instruction should have a dummy read."},
    {'5',"The STA,X dummy read was on an incorrect address."},
    {'6',"LDA (Indirect),Y should not have a dummy read if no page boundary is crossed by Y indexing."},
    {'7',"LDA (Indirect),Y should have a dummy read if a page boundary is crossed by Y indexing."},
    {'8',"STA (Indirect),Y should not have a dummy read if no page boundary is crossed by Y indexing."},
    {'9',"STA (Indirect),Y should have a dummy read if a page boundary is crossed by Y indexing."},
    {'A',"LDA (Indirect,X) should not have a dummy read."},
    {'B',"STA (Indirect,X) should not have a dummy read."},
};
static const AcCode e_DummyWrites[] = {
    {'1',"PPU Open Bus should exist."},
    {'2',"Read-modify-write instructions should write to $2006 twice."},
    {'3',"Read-modify-write instructions with X indexing should write to $2006 twice."},
};
static const AcCode e_OpenBus[] = {
    {'1',"Reading from open bus is not all zeroes."},
    {'2',"Reading from open bus with LDA Absolute should return the high byte of the operand."},
    {'3',"Indexed addressing crossing a page boundary should not update the data bus to the new high byte value."},
    {'4',"Moving the PC to open bus should read instructions from the floating data bus values; write cycles should update the data bus."},
    {'5',"Dummy reads should update the data bus."},
    {'6',"The upper 3 bits when reading from the controller should be open bus."},
    {'7',"Reading from $4015 should not update the databus."},
    {'8',"Writing should always update the databus, even writing to $4015."},
    {'9',"Bit 5 of address $4015 should be open bus."},
};
static const AcCode e_AllNOPs[] = {
    {'1',"Opcode $04 (NOP Zero Page) malfunctioned."},
    {'2',"Opcode $0C (NOP Absolute) malfunctioned."},
    {'3',"Opcode $14 (NOP Zero Page,X) malfunctioned."},
    {'4',"Opcode $1A (NOP Implied) malfunctioned."},
    {'5',"Opcode $1C (NOP Absolute,X) malfunctioned."},
    {'6',"Opcode $34 (NOP Zero Page,X) malfunctioned."},
    {'7',"Opcode $3A (NOP Implied) malfunctioned."},
    {'8',"Opcode $3C (NOP Absolute,X) malfunctioned."},
    {'9',"Opcode $44 (NOP Zero Page) malfunctioned."},
    {'A',"Opcode $54 (NOP Zero Page,X) malfunctioned."},
    {'B',"Opcode $5A (NOP Implied) malfunctioned."},
    {'C',"Opcode $5C (NOP Absolute,X) malfunctioned."},
    {'D',"Opcode $64 (NOP Zero Page) malfunctioned."},
    {'E',"Opcode $74 (NOP Zero Page,X) malfunctioned."},
    {'F',"Opcode $7A (NOP Implied) malfunctioned."},
    {'G',"Opcode $7C (NOP Absolute,X) malfunctioned."},
    {'H',"Opcode $80 (NOP Immediate) malfunctioned."},
    {'I',"Opcode $82 (NOP Immediate) malfunctioned."},
    {'J',"Opcode $89 (NOP Immediate) malfunctioned."},
    {'K',"Opcode $C2 (NOP Immediate) malfunctioned."},
    {'L',"Opcode $D4 (NOP Zero Page,X) malfunctioned."},
    {'M',"Opcode $DA (NOP Implied) malfunctioned."},
    {'N',"Opcode $DC (NOP Absolute,X) malfunctioned."},
    {'O',"Opcode $E2 (NOP Immediate) malfunctioned."},
    {'P',"Opcode $EA (NOP Implied) malfunctioned."},
    {'Q',"Opcode $F4 (NOP Zero Page,X) malfunctioned."},
    {'R',"Opcode $FA (NOP Implied) malfunctioned."},
    {'S',"Opcode $FC (NOP Absolute,X) malfunctioned."},
};
static const AcTest p1[] = {
    AC_TST("ROM is not writable", e_ROMNotWritable),
    AC_TST("RAM Mirroring", e_RAMMirror),
    AC_TST("PC Wraparound", e_PCWrap),
    AC_TST("The Decimal Flag", e_DecimalFlag),
    AC_TST("The B Flag", e_BFlag),
    AC_TST("Dummy read cycles", e_DummyReads),
    AC_TST("Dummy write cycles", e_DummyWrites),
    AC_TST("Open Bus", e_OpenBus),
    AC_TST("All NOP instructions", e_AllNOPs),
};

// ===================== Page 2: Addressing Mode Wraparound =====================
static const AcCode e_AbsIndex[] = {
    {'1',"Absolute indexed addressing did not read from the correct address."},
    {'2',"When indexing with X beyond $FFFF, the instruction should read from the zero page."},
    {'3',"When indexing with Y beyond $FFFF, the instruction should read from the zero page."},
};
static const AcCode e_ZPgIndex[] = {
    {'1',"Zero Page indexed addressing did not read from the correct address."},
    {'2',"When indexing with X beyond $00FF, the instruction should still read from the zero page."},
    {'3',"When indexing with Y beyond $00FF, the instruction should still read from the zero page."},
};
static const AcCode e_Indirect[] = {
    {'1',"JMP (Indirect) did not move the PC to the correct address."},
    {'2',"The address bus should wrap around the page when reading the low and high bytes with indirect addressing."},
};
static const AcCode e_IndX[] = {
    {'1',"Indirect,X addressing did not read from the correct address."},
    {'2',"The indirect indexing should only occur on the zero page, even if X crosses a page boundary."},
    {'3',"The address bus should wrap around the page when reading the low and high bytes with indirect addressing."},
};
static const AcCode e_IndY[] = {
    {'1',"Indirect,Y addressing did not read from the correct address."},
    {'2',"The Y indexing should be able to cross a page boundary, and the high byte should be updated."},
    {'3',"The address bus should wrap around the page when reading the low and high bytes with indirect addressing."},
};
static const AcCode e_Relative[] = {
    {'1',"You should be able to branch from the Zero Page to page $FF."},
    {'2',"You should be able to branch from page $FF to the Zero Page."},
};
static const AcTest p2[] = {
    AC_TST("Absolute Indexed", e_AbsIndex),
    AC_TST("Zero Page Indexed", e_ZPgIndex),
    AC_TST("Indirect", e_Indirect),
    AC_TST("Indirect, X", e_IndX),
    AC_TST("Indirect, Y", e_IndY),
    AC_TST("Relative", e_Relative),
};

// ===================== Pages 3-11: Unofficial Instructions (shared table) =====================
static const AcCode e_Unofficial[] = {
    {'F',"The high byte corruption did not match any known behavior. (Only applicable to SHA and SHS.)"},
    {'0',"This instruction had the wrong number of operand bytes."},
    {'1',"The target address of the instruction was not correct."},
    {'2',"The A register was not the correct value after the test."},
    {'3',"The X register was not the correct value after the test."},
    {'4',"The Y register was not the correct value after the test."},
    {'5',"The CPU status flags were not correct after the test."},
    {'6',"The stack pointer was not the correct value after the test. (Only SHS and LAE)"},
    {'7',"RDY low 2 cycles before write: target address not correct. (SHA, SHX, SHY, SHS)"},
    {'8',"RDY low 2 cycles before write: A register not correct. (SHA, SHX, SHY, SHS)"},
    {'9',"RDY low 2 cycles before write: X register not correct. (SHA, SHX, SHY, SHS)"},
    {'A',"RDY low 2 cycles before write: Y register not correct. (SHA, SHX, SHY, SHS)"},
    {'B',"RDY low 2 cycles before write: CPU status flags not correct. (SHA, SHX, SHY, SHS)"},
    {'C',"RDY low 2 cycles before write: stack pointer not correct. (SHS)"},
};
static const AcCode ok_ShaShs[] = {
    {'1',"Address-Bus-High-Byte-Corruption performed AND upon ABH with both X and A."},
    {'2',"Address-Bus-High-Byte-Corruption performed AND upon ABH with only X."},
    {'3',"Included a magic number ORed with ABH, or did not occur at all."},
};
static const AcTest p3_SLO[] = {
    AC_TST("$03 SLO indirect,X", e_Unofficial),
    AC_TST("$07 SLO zeropage",   e_Unofficial),
    AC_TST("$0F SLO absolute",   e_Unofficial),
    AC_TST("$13 SLO indirect,Y", e_Unofficial),
    AC_TST("$17 SLO zeropage,X", e_Unofficial),
    AC_TST("$1B SLO absolute,Y", e_Unofficial),
    AC_TST("$1F SLO absolute,X", e_Unofficial),
};
static const AcTest p4_RLA[] = {
    AC_TST("$23 RLA indirect,X", e_Unofficial),
    AC_TST("$27 RLA zeropage",   e_Unofficial),
    AC_TST("$2F RLA absolute",   e_Unofficial),
    AC_TST("$33 RLA indirect,Y", e_Unofficial),
    AC_TST("$37 RLA zeropage,X", e_Unofficial),
    AC_TST("$3B RLA absolute,Y", e_Unofficial),
    AC_TST("$3F RLA absolute,X", e_Unofficial),
};
static const AcTest p5_SRE[] = {
    AC_TST("$43 SRE indirect,X", e_Unofficial),
    AC_TST("$47 SRE zeropage",   e_Unofficial),
    AC_TST("$4F SRE absolute",   e_Unofficial),
    AC_TST("$53 SRE indirect,Y", e_Unofficial),
    AC_TST("$57 SRE zeropage,X", e_Unofficial),
    AC_TST("$5B SRE absolute,Y", e_Unofficial),
    AC_TST("$5F SRE absolute,X", e_Unofficial),
};
static const AcTest p6_RRA[] = {
    AC_TST("$63 RRA indirect,X", e_Unofficial),
    AC_TST("$67 RRA zeropage",   e_Unofficial),
    AC_TST("$6F RRA absolute",   e_Unofficial),
    AC_TST("$73 RRA indirect,Y", e_Unofficial),
    AC_TST("$77 RRA zeropage,X", e_Unofficial),
    AC_TST("$7B RRA absolute,Y", e_Unofficial),
    AC_TST("$7F RRA absolute,X", e_Unofficial),
};
static const AcTest p7_AX[] = {
    AC_TST("$83 SAX indirect,X", e_Unofficial),
    AC_TST("$87 SAX zeropage",   e_Unofficial),
    AC_TST("$8F SAX absolute",   e_Unofficial),
    AC_TST("$97 SAX zeropage,Y", e_Unofficial),
    AC_TST("$A3 LAX indirect,X", e_Unofficial),
    AC_TST("$A7 LAX zeropage",   e_Unofficial),
    AC_TST("$AF LAX absolute",   e_Unofficial),
    AC_TST("$B3 LAX indirect,Y", e_Unofficial),
    AC_TST("$B7 LAX zeropage,Y", e_Unofficial),
    AC_TST("$BF LAX absolute,Y", e_Unofficial),
};
static const AcTest p8_DCP[] = {
    AC_TST("$C3 DCP indirect,X", e_Unofficial),
    AC_TST("$C7 DCP zeropage",   e_Unofficial),
    AC_TST("$CF DCP absolute",   e_Unofficial),
    AC_TST("$D3 DCP indirect,Y", e_Unofficial),
    AC_TST("$D7 DCP zeropage,X", e_Unofficial),
    AC_TST("$DB DCP absolute,Y", e_Unofficial),
    AC_TST("$DF DCP absolute,X", e_Unofficial),
};
static const AcTest p9_ISC[] = {
    AC_TST("$E3 ISC indirect,X", e_Unofficial),
    AC_TST("$E7 ISC zeropage",   e_Unofficial),
    AC_TST("$EF ISC absolute",   e_Unofficial),
    AC_TST("$F3 ISC indirect,Y", e_Unofficial),
    AC_TST("$F7 ISC zeropage,X", e_Unofficial),
    AC_TST("$FB ISC absolute,Y", e_Unofficial),
    AC_TST("$FF ISC absolute,X", e_Unofficial),
};
static const AcTest p10_SH[] = {
    AC_TSTV("$93 SHA indirect,Y", e_Unofficial, ok_ShaShs),
    AC_TSTV("$9F SHA absolute,Y", e_Unofficial, ok_ShaShs),
    AC_TSTV("$9B SHS absolute,Y", e_Unofficial, ok_ShaShs),
    AC_TST ("$9C SHY absolute,X", e_Unofficial),
    AC_TST ("$9E SHX absolute,Y", e_Unofficial),
    AC_TST ("$BB LAE absolute,Y", e_Unofficial),
};
static const AcTest p11_Imm[] = {
    AC_TST("$0B ANC Immediate", e_Unofficial),
    AC_TST("$2B ANC Immediate", e_Unofficial),
    AC_TST("$4B ASR Immediate", e_Unofficial),
    AC_TST("$6B ARR Immediate", e_Unofficial),
    AC_TST("$8B ANE Immediate", e_Unofficial),
    AC_TST("$AB LXA Immediate", e_Unofficial),
    AC_TST("$CB AXS Immediate", e_Unofficial),
    AC_TST("$EB SBC Immediate", e_Unofficial),
};

// ===================== Page 12: CPU Interrupts =====================
static const AcCode e_IFlagLatency[] = {
    {'1',"An IRQ should occur when a DMC sample ends, the DMC IRQ is enabled, and the CPU's I flag is clear."},
    {'2',"The IRQ should occur 2 instructions after CLI. (CLI polls for interrupts before cycle 2.)"},
    {'3',"An IRQ should be able to occur 1 cycle after the final cycle of SEI. (SEI polls before cycle 2.)"},
    {'4',"If an IRQ occurs 1 cycle after the final cycle of SEI, the I flag should be set in the pushed values."},
    {'5',"An IRQ should run again after RTI if the interrupt was not acknowledged and I was not set when pushed."},
    {'6',"The IRQ should occur 1 cycle after the final cycle of RTI. (I is pulled before RTI polls.)"},
    {'7',"The IRQ should occur 2 instructions after PLP. (PLP polls for interrupts before cycle 2.)"},
    {'8',"The DMA triggered an IRQ on the wrong CPU cycle."},
    {'9',"Branch instructions should poll for interrupts before cycle 2."},
    {'A',"Branch instructions should not poll for interrupts before cycle 3."},
    {'B',"Branch instructions should poll for interrupts before cycle 4."},
    {'C',"Error code E requires proper PPU open bus emulation; a prerequisite PPU open bus test failed."},
    {'D',"Error code E requires proper open bus emulation; a prerequisite open bus test failed."},
    {'E',"An interrupt polled on the first poll of a branch, cleared, then polled again, should still occur."},
};
static const AcCode e_NmiBrk[] = {
    {'1',"BRK returned to the wrong address."},
    {'2',"Either NMI timing is off, or interrupt hijacking is incorrectly handled."},
};
static const AcCode e_NmiIrq[] = {
    {'1',"Either NMI timing is off, IRQ timing is off, or interrupt hijacking is incorrectly handled."},
};
static const AcTest p12[] = {
    AC_TST("Interrupt flag latency", e_IFlagLatency),
    AC_TST("NMI Overlap BRK", e_NmiBrk),
    AC_TST("NMI Overlap IRQ", e_NmiIrq),
};

// ===================== Page 13: APU Registers and DMA tests =====================
static const AcCode e_DMAOpenBus[] = {
    {'1',"LDA $4000 should not read back $00 if a DMA did not occur."},
    {'2',"The DMC DMA was either on the wrong cycle, or it did not update the data bus."},
};
static const AcCode e_DMA2002R[] = {
    {'1',"Your emulator did not pass the SLO Absolute,X test."},
    {'2',"The DMC DMA was on the wrong cycle, or the halt/alignment cycles did not read from $2002."},
};
static const AcCode ok_DMA2002R[] = {
    {'1',"The DMC Load DMA occured after 2 APU cycles. (The common behavior)"},
    {'2',"The DMC Load DMA occured after 3 APU cycles. (The uncommon behavior)"},
};
static const AcCode e_DMA2007R[] = {
    {'1',"The PPU Read Buffer is not working."},
    {'2',"The DMC DMA was on the wrong cycle, or the halt/alignment cycles did not read from $2007."},
    {'3',"The halt/alignment cycles did not increment the v register of the PPU enough times."},
};
static const AcCode e_DMA2007W[] = {
    {'1',"DMA + $2007 Read did not pass."},
    {'2',"The DMA was not delayed by the write cycle."},
};
static const AcCode e_DMA4015R[] = {
    {'1',"The APU Frame Timer Interrupt Flag was never set."},
    {'2',"The DMC DMA was on the wrong cycle, or the halt/alignment cycles did not read from $4015 (clearing the APU Frame Timer Interrupt Flag)."},
};
static const AcCode e_DMA4016R[] = {
    {'1',"The DMC DMA was on the wrong cycle, or the halt/alignment cycles did not read from $4016, which should have clocked the controller port."},
};
static const AcCode ok_DMA4016R[] = {
    {'1',"The controller was read the way a US-released NES / AV Famicom should read controllers."},
    {'2',"The controller was read the way a Famicom should read controllers."},
};
static const AcCode e_DMABusConflict[] = {
    {'1',"The DMA did not occur on the correct CPU cycle."},
    {'2',"The DMC DMA did not correctly emulate the bus conflict with the APU registers."},
    {'3',"The DMC DMA bus conflict should clear the APU Frame Counter Interrupt Flag."},
};
static const AcCode ok_DMABusConflict[] = {
    {'1',"The controller was read the way a US-released NES should read controllers."},
    {'2',"The controller was read the way a Famicom should read controllers."},
};
static const AcCode e_DMCOAM[] = {
    {'1',"The DMC DMA timing in your emulator is off."},
    {'2',"The overlapping DMAs did not spend the correct number of CPU cycles."},
};
static const AcCode e_ExplicitAbort[] = {
    {'1',"The DMC DMA timing in your emulator is off."},
    {'2',"The aborted DMAs did not spend the correct number of CPU cycles."},
};
static const AcCode e_ImplicitAbort[] = {
    {'1',"The DMC DMA timing in your emulator is off."},
    {'2',"The aborted DMAs did not spend the correct number of CPU cycles."},
    {'3',"The 1-cycle DMA should not get delayed by a write cycle; it just shouldn't occur in that case."},
    {'4',"If the sample was set to keep looping, the DMC DMA timing in your emulator is off."},
};
static const AcCode ok_ImplicitAbort[] = {
    {'1',"The abort behaved the way a mid-1990 or later CPU would behave."},
    {'2',"The abort behaved the way a pre-mid-1990 CPU would behave."},
};
static const AcTest p13[] = {
    AC_TST ("DMA + Open Bus", e_DMAOpenBus),
    AC_TSTV("DMA + $2002 Read", e_DMA2002R, ok_DMA2002R),
    AC_TST ("DMA + $2007 Read", e_DMA2007R),
    AC_TST ("DMA + $2007 Write", e_DMA2007W),
    AC_TST ("DMA + $4015 Read", e_DMA4015R),
    AC_TSTV("DMA + $4016 Read", e_DMA4016R, ok_DMA4016R),
    AC_TSTV("DMC DMA Bus Conflicts", e_DMABusConflict, ok_DMABusConflict),
    AC_TST ("DMC DMA + OAM DMA", e_DMCOAM),
    AC_TST ("Explicit DMA Abort", e_ExplicitAbort),
    AC_TSTV("Implicit DMA Abort", e_ImplicitAbort, ok_ImplicitAbort),
};

// ===================== Page 14: APU Tests =====================
static const AcCode e_APULenCounter[] = {
    {'1',"Reading from $4015 should not state pulse 1 is playing before you write to $4003."},
    {'2',"Reading from $4015 should state pulse 1 is playing after you write to $4003."},
    {'3',"The audio channel should automatically stop playing if you wait for the length counter to expire."},
    {'4',"Writing $80 to $4017 should immediately clock the Length Counter."},
    {'5',"Writing $00 to $4017 should not clock the Length Counter."},
    {'6',"Disabling the audio channel should immediately clear the length counter to zero."},
    {'7',"The length counter shouldn't be set when the channel is disabled."},
    {'8',"If the channel is set to play infinitely, it shouldn't clock the length counter."},
    {'9',"If the channel is set to play infinitely, the length counter should be left unchanged."},
};
static const AcCode e_APULenTable[] = {
    {'1',"Your emulator did not pass APU Length Counter."},
    {'2',"%00000--- to $4003: pulse 1 length counter should be 10."},
    {'3',"%00001--- to $4003: pulse 1 length counter should be 254."},
    {'4',"%00010--- to $4003: pulse 1 length counter should be 20."},
    {'5',"%00011--- to $4003: pulse 1 length counter should be 2."},
    {'6',"%00100--- to $4003: pulse 1 length counter should be 40."},
    {'7',"%00101--- to $4003: pulse 1 length counter should be 4."},
    {'8',"%00110--- to $4003: pulse 1 length counter should be 80."},
    {'9',"%00111--- to $4003: pulse 1 length counter should be 6."},
    {'A',"%01000--- to $4003: pulse 1 length counter should be 160."},
    {'B',"%01001--- to $4003: pulse 1 length counter should be 8."},
    {'C',"%01010--- to $4003: pulse 1 length counter should be 60."},
    {'D',"%01011--- to $4003: pulse 1 length counter should be 10."},
    {'E',"%01100--- to $4003: pulse 1 length counter should be 14."},
    {'F',"%01101--- to $4003: pulse 1 length counter should be 12."},
    {'G',"%01110--- to $4003: pulse 1 length counter should be 26."},
    {'H',"%01111--- to $4003: pulse 1 length counter should be 14."},
    {'I',"%10000--- to $4003: pulse 1 length counter should be 12."},
    {'J',"%10001--- to $4003: pulse 1 length counter should be 16."},
    {'K',"%10010--- to $4003: pulse 1 length counter should be 24."},
    {'L',"%10011--- to $4003: pulse 1 length counter should be 18."},
    {'M',"%10100--- to $4003: pulse 1 length counter should be 48."},
    {'N',"%10101--- to $4003: pulse 1 length counter should be 20."},
    {'O',"%10110--- to $4003: pulse 1 length counter should be 96."},
    {'P',"%10111--- to $4003: pulse 1 length counter should be 22."},
    {'Q',"%11000--- to $4003: pulse 1 length counter should be 192."},
    {'R',"%11001--- to $4003: pulse 1 length counter should be 24."},
    {'S',"%11010--- to $4003: pulse 1 length counter should be 72."},
    {'T',"%11011--- to $4003: pulse 1 length counter should be 26."},
    {'U',"%11100--- to $4003: pulse 1 length counter should be 16."},
    {'V',"%11101--- to $4003: pulse 1 length counter should be 28."},
    {'W',"%11110--- to $4003: pulse 1 length counter should be 32."},
    {'X',"%11111--- to $4003: pulse 1 length counter should be 30."},
};
static const AcCode e_FrameCounterIRQ[] = {
    {'1',"The IRQ flag should be set in 4-step mode with the IRQ flag enabled."},
    {'2',"The IRQ flag should not be set in 4-step mode with the IRQ flag disabled."},
    {'3',"The IRQ flag should not be set in 5-step mode with the IRQ flag enabled."},
    {'4',"The IRQ flag should not be set in 5-step mode with the IRQ flag disabled."},
    {'5',"Reading the IRQ flag should clear the IRQ flag."},
    {'6',"The IRQ flag should be cleared when the APU transitions from a put cycle to a get cycle."},
    {'7',"The IRQ flag should not be cleared when the APU transitions from a get cycle to a put cycle."},
    {'8',"Changing the frame counter to 5-step mode after the flag was set should not clear the flag."},
    {'9',"Disabling the IRQ flag should clear the IRQ flag."},
    {'A',"The IRQ flag was enabled too early. (writing $4017 on an odd CPU cycle.)"},
    {'B',"The IRQ flag was enabled too late. (writing $4017 on an odd CPU cycle.)"},
    {'C',"The IRQ flag was enabled too early. (writing $4017 on an even CPU cycle.)"},
    {'D',"The IRQ flag was enabled too late. (writing $4017 on an even CPU cycle.)"},
    {'E',"Reading $4015 on the last cycle before the IRQ flag is set should not clear it."},
    {'F',"Reading $4015 on the same cycle the IRQ flag is set should not clear it."},
    {'G',"Reading $4015 1 cycle later than the previous test should not clear the IRQ flag."},
    {'H',"Reading $4015 1 cycle later than the previous test should clear the IRQ flag."},
    {'I',"The Frame Counter Interrupt flag should not be set 29827 cycles after resetting the frame counter."},
    {'J',"The Frame Counter Interrupt flag should be set 29828 cycles after resetting, even if suppressing FC IRQs."},
    {'K',"The Frame Counter Interrupt flag should be set 29829 cycles after resetting, even if suppressing FC IRQs."},
    {'L',"The Frame Counter Interrupt flag should not be set 29830 cycles after resetting if suppressing FC IRQs."},
    {'M',"Despite the flag being set for those 2 CPU cycles, if suppressing FC IRQs, an IRQ should not occur."},
    {'N',"The IRQ occurs on the wrong CPU cycle."},
    {'O',"The IRQ occurs on the wrong CPU cycle."},
};
static const AcCode e_FrameCounter4[] = {
    {'1',"The first clock of the length counters was early."},
    {'2',"The first clock of the length counters was late."},
    {'3',"The second clock of the length counters was early."},
    {'4',"The second clock of the length counters was late."},
    {'5',"The third clock of the length counters was early."},
    {'6',"The third clock of the length counters was late."},
};
static const AcCode e_DMC[] = {
    {'1',"Reading $4015 should set bit 4 when the DMC is playing and clear it when the sample ends."},
    {'2',"Restarting the DMC should re-load the sample length."},
    {'3',"Writing $10 to $4015 should start playing a new sample if the previous one ended."},
    {'4',"Writing $10 to $4015 while a sample is currently playing shouldn't affect anything."},
    {'5',"Writing $00 to $4015 should immediately stop the sample."},
    {'6',"Writing to $4013 shouldn't change the sample length of the currently playing sample."},
    {'7',"The DMC IRQ flag should not be set when disabled."},
    {'8',"The DMC IRQ flag should be set when enabled, and a sample ends."},
    {'9',"Reading $4015 should not clear the IRQ flag."},
    {'A',"Writing to $4015 should clear the IRQ flag."},
    {'B',"Disabling the IRQ flag should clear the IRQ flag."},
    {'C',"Looping samples should loop."},
    {'D',"Looping samples should not set the IRQ flag when they loop."},
    {'E',"Clearing the looping flag and then setting it again should keep the sample looping."},
    {'F',"Clearing the looping flag should not immediately end the sample; it should play for its remaining bytes."},
    {'G',"A looping sample should reload the sample length from $4013 every time the sample loops."},
    {'H',"Writing $00 to $4013 should result in the following sample being 1 byte long."},
    {'I',"There should be a one-byte buffer that's filled immediately if empty."},
    {'J',"The DMA occurred on the wrong CPU cycle."},
    {'K',"The sample address should overflow to $8000 instead of $0000."},
    {'L',"Writing $4015 when the DMC timer has 2 cycles until clocked should not trigger a DMC DMA until after the 3/4 cycle delay."},
    {'M',"Writing $4015 when the DMC timer has 1 cycle until clocked should not trigger a DMC DMA until after the 3/4 cycle delay."},
    {'N',"Writing $4015 when the DMC timer has 0 cycles until clocked should not trigger a DMC DMA until after the 3/4 cycle delay."},
};
static const AcCode e_APURegAct[] = {
    {'1',"A series of prerequisite tests failed. CPU/PPU open bus, PPU Read Buffer, DMA + Open Bus, DMA + $2007 Read."},
    {'2',"There were unexpected extra bits when reading from a controller port that should not be set."},
    {'3',"Reading from $4015 should clear the APU Frame Counter Interrupt flag."},
    {'4',"OAM DMA should not read from APU registers if $40 is written to $4016 and the CPU Address Bus is not $4000-$401F."},
    {'5',"Something went wrong during open bus execution. Controller port 2 was possibly clocked too many times."},
    {'6',"OAM DMA should be able to read from APU registers (and mirrors) if $40 is written to $4016 and the bus is $4000-$401F."},
    {'7',"Bus conflicts with the APU registers were not properly emulated."},
};
static const AcCode ok_APURegAct[] = {
    {'1',"The controllers were not clocked by the bus conflict with the OAM DMA."},
    {'2',"The controllers were clocked by the bus conflict with the OAM DMA."},
};
static const AcCode e_CtrlStrobing[] = {
    {'1',"A value of $02 written to $4016 should not strobe the controllers."},
    {'2',"Any value with bit 0 set written to $4016 should strobe the controllers."},
    {'3',"Controllers should be strobed when the CPU transitions from a get cycle to a put cycle."},
    {'4',"Controllers should not be strobed when the CPU transitions from a put cycle to a get cycle."},
};
static const AcCode e_CtrlClocking[] = {
    {'1',"Reading $4016 more than 8 times should always result in bit 0 being set to 1."},
    {'2',"Reading a controller port while still strobed shouldn't affect the shift register (constantly reloaded)."},
    {'3',"Your emulator did not pass the SLO Absolute,X test."},
    {'4',"(NES/AV Famicom only) Double-reading $4016 should only clock the controller once."},
    {'5',"(NES/AV Famicom only) This double-read should be the same value for both reads."},
    {'6',"(NES/AV Famicom only) The put/halt cycles of the DMC DMA should clock the controller if the DMA occurs during a $4016 read."},
    {'7',"(NES/AV Famicom only) If the DMC DMA get has a bus conflict with $4016, the controller clocks only once during LDA $4016."},
};
static const AcCode ok_CtrlClocking[] = {
    {'1',"The controller was read the way a US-released NES / AV Famicom should read controllers."},
    {'2',"The controller was read the way a Famicom should read controllers."},
};
static const AcTest p14[] = {
    AC_TST ("Length Counter", e_APULenCounter),
    AC_TST ("Length Table", e_APULenTable),
    AC_TST ("Frame Counter IRQ", e_FrameCounterIRQ),
    AC_TST ("Frame Counter 4-step", e_FrameCounter4),
    AC_TST ("Frame Counter 5-step", e_FrameCounter4),
    AC_TST ("Delta Modulation Channel", e_DMC),
    AC_TSTV("APU Register Activation", e_APURegAct, ok_APURegAct),
    AC_TST ("Controller Strobing", e_CtrlStrobing),
    AC_TSTV("Controller Clocking", e_CtrlClocking, ok_CtrlClocking),
};

// ===================== Page 15: Power On State (DRAW only) =====================
static const AcTest p15[] = {
    AC_DRAW("PPU Reset Flag"),
    AC_DRAW("CPU RAM"),
    AC_DRAW("CPU Registers"),
    AC_DRAW("PPU RAM"),
    AC_DRAW("Palette RAM"),
};

// ===================== Page 16: PPU Behavior =====================
static const AcCode e_CHRROM[] = {
    {'1',"Writes to $0000-$1FFF should not overwrite CHR data if the cartridge has CHR ROM instead of CHR RAM."},
};
static const AcCode e_PPURegMirror[] = {
    {'1',"PPU registers should be mirrored through $3FFF."},
};
static const AcCode e_PPUOpenBus[] = {
    {'1',"Reading from a write-only PPU register should return the most recently written value to the PPU data bus."},
    {'2',"All PPU registers should update the PPU data bus when written."},
    {'3',"Bits 0 through 4 when reading from $2002 should read the PPU data bus."},
    {'4',"Reads from $2002 should update the upper 3 bits of the PPU data bus."},
    {'5',"The PPU data bus value should decay before 1 second passes."},
};
static const AcCode e_PPUReadBuffer[] = {
    {'1',"Reading from the PPU register at $2007 is not working at all in this emulator."},
    {'2',"Reading address $2007 should increment the v register."},
    {'3',"There should be a 1-byte buffer when reading from $2007."},
    {'4',"Reading from CHR ROM should use the buffer."},
    {'5',"Writing to $2006 does not modify the buffer value."},
    {'6',"Reading from Palette RAM should NOT use the buffer."},
    {'7',"The value on the nametable at $2F00-$2FFF should be put in the buffer when reading palette RAM at $3F00-$3FFF."},
};
static const AcCode ok_PPUReadBuffer[] = {
    {'E',"The PPU behaved like revision E or earlier."},
    {'G',"The PPU behaved like revision G or earlier."},
};
static const AcCode e_PaletteRAMQuirks[] = {
    {'1',"This emulator failed the PPU Read Buffer test."},
    {'2',"Palette RAM should be mirrored through $3FFF."},
    {'3',"The backdrop colors for palettes 1,2,3 should not be mirrors of the backdrop color of palette 0."},
    {'4',"The backdrop colors for sprites should be mirrors of the backdrop colors for backgrounds."},
    {'5',"Values read from Palette RAM should only be 6-bit, with the upper 2 bits being PPU open bus."},
    {'6',"With Greyscale Mode enabled, the lower four bits of the value read should all be zero."},
    {'7',"With Greyscale Mode enabled, the lower four bits of the value written should be unaffected."},
};
static const AcCode e_RenderingFlag[] = {
    {'1',"Background shift registers should not be initialized or clocked when rendering is entirely disabled."},
    {'2',"Background shift registers should be initialized and clocked when only rendering sprites."},
    {'3',"Sprite Evaluation should still occur when only rendering the background."},
};
static const AcCode e_2007Rendering[] = {
    {'1',"Sprite Zero Hits should be working."},
    {'2',"Reading from $2007 while rendering is enabled should result in a vertical increment of v."},
};
static const AcTest p16[] = {
    AC_TST ("CHR ROM is not writable", e_CHRROM),
    AC_TST ("PPU Register Mirroring", e_PPURegMirror),
    AC_TST ("PPU Register Open Bus", e_PPUOpenBus),
    AC_TSTV("PPU Read Buffer", e_PPUReadBuffer, ok_PPUReadBuffer),
    AC_TST ("Palette RAM Quirks", e_PaletteRAMQuirks),
    AC_TST ("Rendering Flag Behavior", e_RenderingFlag),
    AC_TST ("$2007 read w/ rendering", e_2007Rendering),
};

// ===================== Page 17: PPU VBlank Timing =====================
static const AcCode e_VBlankBegin[] = {
    {'1',"The PPU $2002 VBlank flag was not set at the correct PPU cycle."},
};
static const AcCode e_VBlankEnd[] = {
    {'1',"The PPU $2002 VBlank flag was not cleared at the correct PPU cycle."},
};
static const AcCode e_NMIControl[] = {
    {'1',"The NMI should not occur when disabled."},
    {'2',"The NMI should occur at VBlank when enabled."},
    {'3',"The NMI should occur when enabled during VBlank, if the VBlank flag is enabled."},
    {'4',"The NMI should not occur when enabled during VBlank, if the VBlank flag is disabled."},
    {'5',"The NMI should not occur a second time if writing $80 to $2000 when the NMI flag is already enabled."},
    {'6',"The NMI should not occur a second time if writing $80 to $2000 when already enabled, going into VBlank."},
    {'7',"The NMI should occur an additional time if you disable and then re-enable the NMI."},
    {'8',"The NMI is polled before the write cycle of STA (gap between enabling NMI and the NMI occurring)."},
    {'9',"The NMI is polled between the write cycles of INC (NMI occurs immediately after the INC)."},
};
static const AcCode e_NMITiming[] = {
    {'1',"The NMI did not occur on the correct PPU cycle."},
};
static const AcCode e_NMISuppression[] = {
    {'1',"The NMI did not occur on the correct PPU cycle, or was not suppressed by a precisely timed read of $2002."},
};
static const AcCode e_NMIVBLEnd[] = {
    {'1',"The NMI could occur too late or was disabled too early."},
};
static const AcCode e_NMIDisabledVBL[] = {
    {'1',"The NMI could occur too late or was disabled too early."},
};
static const AcTest p17[] = {
    AC_TST("VBlank beginning", e_VBlankBegin),
    AC_TST("VBlank end", e_VBlankEnd),
    AC_TST("NMI Control", e_NMIControl),
    AC_TST("NMI Timing", e_NMITiming),
    AC_TST("NMI Suppression", e_NMISuppression),
    AC_TST("NMI at VBlank end", e_NMIVBLEnd),
    AC_TST("NMI disabled at VBlank", e_NMIDisabledVBL),
};

// ===================== Page 18: Sprite Evaluation =====================
static const AcCode e_SprOverflow[] = {
    {'1',"Evaluating 9 sprites in a single scanline should set the Sprite Overflow Flag."},
    {'2',"The Sprite Overflow Flag should not be the same thing as the CPU's Overflow flag."},
    {'3',"Evaluating only 8 sprites in a single scanline should not set the Sprite Overflow Flag."},
    {'4',"Sprite evaluation should occur even if only the background is being rendered (also setting the Overflow Flag)."},
};
static const AcCode e_Sprite0Hit[] = {
    {'1',"A Sprite zero hit did not occur."},
    {'2',"Sprite zero hits should not happen if background rendering is disabled."},
    {'3',"Sprite zero hits should not happen if sprite rendering is disabled."},
    {'4',"Sprite zero hits should not happen if both sprites and background rendering are disabled."},
    {'5',"Sprite zero hits should not happen if sprite zero is completely transparent."},
    {'6',"Sprite zero hits should be able to happen at X=254."},
    {'7',"Sprite zero hits should not be able to happen at X=255."},
    {'8',"Sprite zero hits should not happen if sprite zero is at X=0 and the 8 pixel mask is enabled (show BG, no sprite)."},
    {'9',"Sprite zero hits should not happen if sprite zero is at X=0 and the 8 pixel mask is enabled (show sprite, no BG)."},
    {'A',"Despite the 8 pixel mask, if the sprite has visible pixels beyond the mask the hit should occur."},
    {'B',"Sprite zero hits should be able to happen at Y=238."},
    {'C',"Sprite zero hits should not be able to happen at Y>=239."},
    {'D',"Your sprites are rendered one scanline too high, or your hit detection isn't checking for solid overlapping pixels."},
    {'E',"The sprite zero hit flag was set too early."},
};
static const AcCode e_2002FlagTiming[] = {
    {'1',"The flags were not cleared on the correct PPU cycle."},
    {'2',"The flags were not set on the correct PPU cycle."},
};
static const AcCode e_SuddenlyResize[] = {
    {'1',"Sprite Zero Hits should be working."},
    {'2',"Writing $2000 to enable 16px sprites at the start of HBlank should let an otherwise out-of-range 8px sprite extend into the scanline."},
    {'3',"Same as code 2, but writes after sprite zero would be determined out-of-range; data should not exist in the shift registers."},
    {'4',"Writing $2000 to disable 16px sprites at the start of HBlank should prevent an in-range 16px sprite from extending into the scanline."},
    {'5',"Same as code 4, but writes after sprite zero prepared; data should still exist in the shift registers."},
};
static const AcCode e_ArbitrarySpr0[] = {
    {'1',"Sprite 0 should trigger a sprite zero hit. No other sprite should."},
    {'2',"The first processed sprite of a scanline should be treated as sprite zero."},
    {'3',"Misaligned OAM should be able to trigger a sprite zero hit."},
};
static const AcCode e_MisalignedOAM[] = {
    {'1',"Misaligned OAM should be able to trigger a sprite zero hit."},
    {'2',"Misaligned OAM should stay misaligned until an object's Y is out of range, then OAMADDR += 4 and AND $FC."},
    {'3',"If Secondary OAM is full when Y is out of range, only increment the OAM address by 5 (instead of +4 & $FC)."},
    {'4',"Misaligned OAM should realign if an object's X is out of range, then OAMADDR += 1 and AND $FC."},
    {'5',"A combination of tests 3 and 4 occuring on the same scanline."},
    {'6',"Same as test 4, but initial OAM address was $02 instead of $01 (possible false positive on test 4)."},
    {'7',"Same as test 5, but initial OAM address was $03 instead of $01 (possible false positive on test 5)."},
};
static const AcCode e_Address2004[] = {
    {'1',"Writes to $2004 should update OAM and increment the OAM address by 1."},
    {'2',"Reads from $2004 should give you a value in OAM, but do not increment the OAM address."},
    {'3',"Reads from the attribute bytes should be missing bits 2 through 4."},
    {'4',"Reads from $2004 during PPU cycles 1-64 of a visible scanline (rendering enabled) should read $FF."},
    {'5',"Reads from $2004 during PPU cycles 1-64 of a visible scanline (rendering disabled) should do a regular read."},
    {'6',"Writing to $2004 on a visible scanline should increment the OAM address by 4."},
    {'7',"Writing to $2004 on a visible scanline shouldn't write to OAM."},
    {'8',"Reads from $2004 during PPU cycles 65-256 of a visible scanline (rendering enabled) should read the current OAM address."},
    {'9',"Reads from $2004 during PPU cycles 256-320 of a visible scanline (rendering enabled) should read $FF."},
    {'A',"Writing to $2004 on a visible scanline should increment OAMADDR by 4, then AND with $FC."},
};
static const AcCode ok_Address2004[] = {
    {'E',"The PPU behaved like revision E or earlier."},
    {'G',"The PPU behaved like revision G or earlier."},
};
static const AcCode e_OAMCorruption[] = {
    {'1',"This emulator failed to sync the CPU to VBlank during a test that ran when the ROM boots."},
    {'2',"OAM Corruption should corrupt a row in OAM by copying the 8 values from row 0 to another row."},
    {'3',"This corruption should not occur immediately after disabling rendering."},
    {'4',"This corruption should not occur immediately after re-enabling rendering."},
};
static const AcCode e_INC4014[] = {
    {'1',"The DMC DMA should update the data bus."},
    {'2',"The OAM DMA should use the value of the second write to $4014 as the page number (requires precise DMC DMA timing)."},
    {'3',"Only a single OAM DMA should occur despite two writes to $4014."},
};
static const AcTest p18[] = {
    AC_TST ("Sprite overflow behavior", e_SprOverflow),
    AC_TST ("Sprite 0 Hit behavior", e_Sprite0Hit),
    AC_TST ("$2002 flag timing", e_2002FlagTiming),
    AC_TST ("Suddenly Resize Sprite", e_SuddenlyResize),
    AC_TST ("Arbitrary Sprite zero", e_ArbitrarySpr0),
    AC_TST ("Misaligned OAM behavior", e_MisalignedOAM),
    AC_TSTV("Address $2004 behavior", e_Address2004, ok_Address2004),
    AC_TST ("OAM Corruption", e_OAMCorruption),
    AC_TST ("INC $4014", e_INC4014),
};

// ===================== Page 19: PPU Misc. =====================
static const AcCode e_AttribTiles[] = {
    {'1',"Moving the PPU t register to an attribute table should render attribute bytes as tile data in scanlines 0-15."},
    {'2',"With t pointing to an attribute table, scanlines 16-239 should be from the same nametable as the attributes."},
};
static const AcCode e_tRegQuirks[] = {
    {'1',"Sprite Zero Hits should be working."},
    {'2',"Writing to $2006 should overwrite some of the bits set up by writing to $2005."},
    {'3',"Writes to $2005 and $2006 should use the same write latch (single $2006 write then $2005)."},
    {'4',"Writes to $2005 and $2006 should use the same write latch (single $2005 write then $2006)."},
    {'5',"Writing to $2000 between writes to $2006 should still set the nametable select bits of t."},
};
static const AcCode e_StaleBG[] = {
    {'1',"Sprite Zero Hits should be working."},
    {'2',"Sprite zero hits shouldn't occur if sprite zero isn't overlapping a solid pixel."},
    {'3',"BG shift registers should not be clocked during H-Blank or F-Blank; a hit should occur on stale data after re-enabling."},
    {'4',"The sprite shifters should treat all sprite X positions as 0 if rendering disabled and stays so during dot 339."},
};
static const AcCode e_StaleSprite[] = {
    {'1',"Sprite Zero Hits should be working."},
    {'2',"Sprite counters should continue clocking during F-Blank."},
    {'3',"The sprite shift registers should not be clocked during F-Blank or H-Blank."},
    {'4',"Sprite Zero hits shouldn't occur at X=$FF."},
    {'5',"Sprites should be drawn as soon as rendering is enabled if shifters reset during H-Blank but dot 339 was F-Blank."},
    {'6',"F-Blank should prevent shifters/counters from reloading during H-Blank, drawing the sprite once rendering re-enables."},
};
static const AcCode e_BGSerialIn[] = {
    {'1',"Sprite zero hits should not occur when the nametable is entirely blank."},
    {'2',"BG shift registers should bring in a 1 into bit 0 when shifted (visible via timed $2001 writes)."},
};
static const AcCode e_Scanline0Spr[] = {
    {'1',"Sprites at Y=0 should actually be drawn at Y=1."},
    {'2',"A sprite should be drawn at Y=0 via the pre-render scanline's sprite fetch with stale secondary OAM data."},
    {'3',"(RGB PPU) Sprite zero hits should not occur at X=$00 during this test on an RGB PPU. / (Composite PPU) Sprites on scanline zero with non-zero X draw a single pixel at X=0 on frames after the pre-render line skips a cycle."},
};
static const AcCode ok_Scanline0Spr[] = {
    {'1',"This test was ran on a composite PPU."},
    {'2',"This test was ran on an RGB PPU."},
};
static const AcCode e_2004Stress[] = {
    {'1',"This emulator failed to sync the CPU to VBlank during a test that ran when the ROM boots."},
    {'2',"Reading from $2004 (rendering enabled) should read from the OAM Buffer; results didn't match (OAMADDR overflow case)."},
    {'3',"Reading from $2004 (rendering enabled) should read from the OAM Buffer; results didn't match (>8 in-range objects)."},
};
static const AcCode e_2007Stress[] = {
    {'1',"This emulator failed to sync the CPU to VBlank during a test that ran when the ROM boots."},
    {'2',"Reading from $2007 should set up the PPU Read Buffer two PPU cycles after the CPU read ends; likely wrong PPU cycle or missing dummy nametable reads."},
};
static const AcCode e_ALERead[] = {
    {'1',"Sprite Zero Hits should be working."},
    {'2',"A well timed read from $2007 should affect the PPU Address Bus during the BG read cadence, reading a bit plane from an unintended address."},
};
static const AcTest p19[] = {
    AC_TST ("Attributes As Tiles", e_AttribTiles),
    AC_TST ("t Register Quirks", e_tRegQuirks),
    AC_TST ("Stale BG Shift Registers", e_StaleBG),
    AC_TST ("Stale Sprite Shift Regs", e_StaleSprite),
    AC_TST ("BG Serial In", e_BGSerialIn),
    AC_TSTV("Sprites On Scanline 0", e_Scanline0Spr, ok_Scanline0Spr),
    AC_TST ("$2004 Stress Test", e_2004Stress),
    AC_TST ("$2007 Stress Test", e_2007Stress),
    AC_TST ("ALE + Read", e_ALERead),
};

// ===================== Page 20: CPU Behavior 2 =====================
static const AcCode e_InstrTiming[] = {
    {'1',"The DMA should update the data bus."},
    {'2',"The DMA timing is not accurate enough to test this."},
    {'3',"Immediate addressed instructions should take 2 CPU cycles."},
    {'4',"Zero page addressing for non-RMW instructions should take 3 cycles."},
    {'5',"Zero page addressing for RMW instructions should take 5 cycles."},
    {'6',"Indexed zero page for non-RMW instructions should take 4 cycles."},
    {'7',"Indexed zero page for RMW instructions should take 6 cycles."},
    {'8',"Absolute addressing for non-RMW instructions should take 4 cycles."},
    {'9',"Absolute addressing for RMW instructions should take 6 cycles."},
    {'A',"Indexed absolute for STA should always take 5 cycles."},
    {'B',"Indexed absolute for many instructions should take an extra cycle if a page boundary is crossed."},
    {'C',"Indexed absolute for RMW instructions should always take 7 cycles."},
    {'D',"Indirect,X instructions should always take 6 cycles (except some unofficial ones)."},
    {'E',"Indirect,Y instructions should take an extra cycle if a page boundary is crossed."},
    {'F',"Implied instructions should take 2 cycles."},
    {'G',"PHP should take 3 cycles."},
    {'H',"PHA should take 3 cycles."},
    {'I',"PLP should take 4 cycles."},
    {'J',"PLA should take 4 cycles."},
    {'K',"JMP should take 3 cycles."},
    {'L',"JSR should take 6 cycles."},
    {'M',"RTS should take 6 cycles."},
    {'N',"RTI should take 6 cycles."},
    {'O',"BRK should take 7 cycles."},
    {'P',"JMP (indirect) should take 5 cycles."},
};
static const AcCode e_ImpliedDummy[] = {
    {'0',"Your emulator did not pass the SLO Absolute,X test."},
    {'1',"There were unexpected extra bits when reading from a controller port that should not be set."},
    {'2',"Your emulator did not implement the frame counter interrupt flag properly."},
    {'3',"Your emulator did not update the data bus when the DMC DMA occured, or your DMA timing is off."},
    {'4',"Your emulator did not correctly emulate open bus behavior. (Or if it crashes here, the cycles of JSR are in the wrong order.)"},
    {'5',"ASL A should perform a dummy read on cycle 2."},
    {'6',"CLC should perform a dummy read on cycle 2."},
    {'7',"LSR A should perform a dummy read on cycle 2."},
    {'8',"CLI should perform a dummy read on cycle 2."},
    {'9',"DEY should perform a dummy read on cycle 2."},
    {'A',"TXA should perform a dummy read on cycle 2."},
    {'B',"TYA should perform a dummy read on cycle 2."},
    {'C',"TXS should perform a dummy read on cycle 2."},
    {'D',"INY should perform a dummy read on cycle 2."},
    {'E',"DEX should perform a dummy read on cycle 2."},
    {'F',"CLD should perform a dummy read on cycle 2."},
    {'G',"ROL A should perform a dummy read on cycle 2."},
    {'H',"SEC should perform a dummy read on cycle 2."},
    {'I',"ROR A should perform a dummy read on cycle 2."},
    {'J',"SEI should perform a dummy read on cycle 2."},
    {'K',"TAY should perform a dummy read on cycle 2."},
    {'L',"TAX should perform a dummy read on cycle 2."},
    {'M',"CLV should perform a dummy read on cycle 2."},
    {'N',"TSX should perform a dummy read on cycle 2."},
    {'O',"INX should perform a dummy read on cycle 2."},
    {'P',"SED should perform a dummy read on cycle 2."},
    {'Q',"NOP should perform a dummy read on cycle 2."},
    {'R',"PHP should perform a dummy read on cycle 2."},
    {'S',"PHA should perform a dummy read on cycle 2."},
    {'T',"PLP should perform a dummy read on cycle 2."},
    {'U',"PLA should perform a dummy read on cycle 2."},
    {'V',"BRK should perform a dummy read on cycle 2."},
    {'W',"RTI should perform a dummy read on cycle 2."},
    {'X',"RTS should perform a dummy read on cycle 2."},
    {'Y',"RTS should perform a dummy read on cycle 6."},
};
static const AcCode e_BranchDummy[] = {
    {'1',"Your emulator does not accurately emulate RAM Mirroring."},
    {'2',"Your emulator does not accurately emulate the PPU Open Bus."},
    {'3',"Your emulator does not accurately emulate reads from address $2004."},
    {'4',"The third CPU cycle of branch instructions should dummy read from the byte following the operand."},
    {'5',"The fourth CPU cycle of branch instructions (crossing a page) should dummy read before correcting the high byte."},
};
static const AcCode e_JSREdge[] = {
    {'1',"Your emulator pushed the wrong value for the return address."},
    {'2',"JSR should push the return address to the stack between reading the first and second operand."},
    {'3',"Your emulator has incorrect open bus emulation."},
    {'4',"JSR should leave the value of the second operand on the data bus."},
};
static const AcCode e_InternalDataBus[] = {
    {'1',"Reading from open bus should work correctly when crossing a page boundary. DMC DMA timing should be correct."},
    {'2',"The DMC DMA bus conflict with $4015 cannot affect the internal data bus. / Reads from $4015 only update the internal data bus and cannot affect the external data bus."},
};
static const AcTest p20[] = {
    AC_TST("Instruction Timing", e_InstrTiming),
    AC_TST("Implied Dummy Reads", e_ImpliedDummy),
    AC_TST("Branch Dummy Reads", e_BranchDummy),
    AC_TST("JSR Edge Cases", e_JSREdge),
    AC_TST("Internal Data Bus", e_InternalDataBus),
};

// ===================== Page table =====================
static const AcPage AC_PAGES[20] = {
    {"CPU Behavior",                 p1,  (int)(sizeof(p1)/sizeof(p1[0]))},
    {"Addressing Mode Wraparound",   p2,  (int)(sizeof(p2)/sizeof(p2[0]))},
    {"Unofficial Instructions: SLO", p3_SLO, (int)(sizeof(p3_SLO)/sizeof(p3_SLO[0]))},
    {"Unofficial Instructions: RLA", p4_RLA, (int)(sizeof(p4_RLA)/sizeof(p4_RLA[0]))},
    {"Unofficial Instructions: SRE", p5_SRE, (int)(sizeof(p5_SRE)/sizeof(p5_SRE[0]))},
    {"Unofficial Instructions: RRA", p6_RRA, (int)(sizeof(p6_RRA)/sizeof(p6_RRA[0]))},
    {"Unofficial Instructions: *AX", p7_AX,  (int)(sizeof(p7_AX)/sizeof(p7_AX[0]))},
    {"Unofficial Instructions: DCP", p8_DCP, (int)(sizeof(p8_DCP)/sizeof(p8_DCP[0]))},
    {"Unofficial Instructions: ISC", p9_ISC, (int)(sizeof(p9_ISC)/sizeof(p9_ISC[0]))},
    {"Unofficial Instructions: SH*", p10_SH, (int)(sizeof(p10_SH)/sizeof(p10_SH[0]))},
    {"Unofficial Immediates",        p11_Imm,(int)(sizeof(p11_Imm)/sizeof(p11_Imm[0]))},
    {"CPU Interrupts",               p12, (int)(sizeof(p12)/sizeof(p12[0]))},
    {"APU Registers and DMA tests",  p13, (int)(sizeof(p13)/sizeof(p13[0]))},
    {"APU Tests",                    p14, (int)(sizeof(p14)/sizeof(p14[0]))},
    {"Power On State (DRAW)",        p15, (int)(sizeof(p15)/sizeof(p15[0]))},
    {"PPU Behavior",                 p16, (int)(sizeof(p16)/sizeof(p16[0]))},
    {"PPU VBlank Timing",            p17, (int)(sizeof(p17)/sizeof(p17[0]))},
    {"Sprite Evaluation",            p18, (int)(sizeof(p18)/sizeof(p18[0]))},
    {"PPU Misc.",                    p19, (int)(sizeof(p19)/sizeof(p19[0]))},
    {"CPU Behavior 2",               p20, (int)(sizeof(p20)/sizeof(p20[0]))},
};

inline const char* acLookup(const AcCode* table, int n, char code) {
    for (int i = 0; i < n; ++i)
        if (table[i].code == code) return table[i].text;
    return nullptr;
}

