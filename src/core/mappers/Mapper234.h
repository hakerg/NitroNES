#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 234 - Maxi 15
// ----------------------------------------------------------------------------
// Rejestry w $FF80-$FFFF, mask $FFF8. Bankswitch dziala TAKZE przy odczycie
// (zachowuje sie jak Atari 2600 bank-on-access).
//   $FF80,$FF88,$FF90,$FF98 -> Reg 0: [MOQq BBBb]
//     M = mirror (0=V,1=H), O = mode (0=CNROM,1=NINA-03), B/b = block,
//     Q = ROM switch (0=ROMs 1+2), q = ROMs 3+4 disable
//   $FFC0,$FFC8,$FFD0,$FFD8 -> Reg 1: [.... ..LL] lockout (ignorujemy)
//   $FFE8,$FFF0             -> Reg 2: [.cCC ...P] CHR/PRG page
// Reg 0 i 1 sa lockowane gdy bottom 6 bitow Reg 0 sa nie-zero (do resetu).
//
// CHR (8K @ $0000):
//   O=0: page = %BB BbCC      O=1: page = %BB BcCC
// PRG (32K @ $8000):
//   O=0: page = %BBBb         O=1: page = %BBBP
// ----------------------------------------------------------------------------
class Mapper234 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        reg0 = 0;
        reg2 = 0;
        locked = false;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override {
        // Najpierw mapowanie - poniewaz read tez moze zmieniac rejestry
        uint8_t bank = currentPrg();
        bool inRom = addr >= 0x8000;
        uint32_t outMapped = 0;
        if (inRom)
            outMapped = (uint32_t)bank * 0x8000 + (addr & 0x7FFF);

        bankswitchOnAccess(addr);

        if (!inRom)
            return false;
        mapped = outMapped;
        (void)data;
        return true;
    }
    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        writeReg(addr, data);
        bankswitchOnAccess(addr);
    }
    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t bank = currentChr();
        mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override {
        return (reg0 & 0x80) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
    }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t reg0 = 0;
    uint8_t reg2 = 0;
    bool locked = false;

    // Reg 2 (cCC..P) jest "nigdy nie lockowane".
    // reg0: [MOQq BBBb], reg2: [.cCC ...P]
    uint8_t currentPrg() const {
        uint8_t B = (reg0 >> 1) & 0x07; // BBB
        uint8_t b = reg0 & 0x01;        // b
        bool O = (reg0 & 0x40) != 0;
        uint8_t bank;
        if (!O)
            bank = (uint8_t)((B << 1) | b);
        else
            bank = (uint8_t)((B << 1) | (reg2 & 0x01));
        // Q (bit 5) selects ROMs 3+4; ignorujemy (kartridze dystrybuowano tylko
        // z 1+2).
        return bank;
    }
    uint8_t currentChr() const {
        uint8_t B = (reg0 >> 1) & 0x07;
        uint8_t b = reg0 & 0x01;
        uint8_t CC = (reg2 >> 4) & 0x03;
        uint8_t c = (reg2 >> 6) & 0x01;
        bool O = (reg0 & 0x40) != 0;
        if (!O)
            return (uint8_t)((B << 4) | (b << 3) | (CC << 1) | (CC >> 1));
        // O=1: %BB BcCC -> bits: BB(2) Bc(2) CC(2)
        // Realnie wzor pages: %BB BcCC (5-bit nieliniowo). Aproksymacja:
        return (uint8_t)((B << 4) | (b << 3) | (c << 2) | CC);
    }

    void writeReg(uint16_t addr, uint8_t data) {
        uint16_t a = addr & 0xFFF8;
        if (a >= 0xFF80 && a <= 0xFF98) {
            if (!locked) {
                reg0 = data;
                updateLock();
            }
        } else if (a >= 0xFFC0 && a <= 0xFFD8) {
            if (!locked) {
                // reg1 lockout defeat - ignore
            }
        } else if (a == 0xFFE8 || a == 0xFFF0) {
            reg2 = data; // never locked
        }
    }
    void bankswitchOnAccess(uint16_t addr) {
        // Wedlug specs bankswitch dzieje sie na read TEZ; faktyczna logika jest
        // taka, ze adres z $FF80-$FFFF probuje przepisac rejestry przy odczycie
        // uzywajac bytea z ROM-u. Nie mamy tu danych z ROMu, wiec dla
        // uproszczenia robimy nic na READ - efekt write powyzej wystarcza dla
        // wiekszosci sekwencji LDA #$xx / STA $FFxx. Funkcja jest zostawiona
        // jako miejsce na pelniejsze wsparcie.
        (void)addr;
    }
    void updateLock() {
        // "Once the bottom 6 bits of Reg 0 contain a non-zero value, Reg 0 and
        // Reg 1
        //  are locked and cannot be changed until the system is reset."
        if ((reg0 & 0x3F) != 0)
            locked = true;
    }
    const char* name() const override { return "Maxi 15"; }
};

REGISTER_MAPPER(234, Mapper234)
