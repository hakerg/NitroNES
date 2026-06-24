#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 095 - MMC3 modified (Namcot 119 / Dragon Buster J)
// ----------------------------------------------------------------------------
// Identyczny jak MMC3, ale mirroring nie pochodzi z $A000.
// Zamiast tego bit 5 banków CHR (R:0..R:5) wybiera NTA/NTB.
// Tryb CHR (bit 7 z $8000) decyduje, ktore rejestry mapuja ktora polowe.
// ----------------------------------------------------------------------------
class Mapper095 : public Mapper {
public:
    Mapper095(uint8_t prg, uint8_t chr) : Mapper(prg, chr) {
        pPRGBank.fill(0);
        pCHRBank.fill(0);
        reset();
    }

    void reset() override {
        targetReg = 0;
        prgMode = false;
        chrInversion = false;
        ntA = 0;
        ntB = 0;
        ntC = 0;
        ntD = 0;
        for (auto &r : pRegister)
            r = 0;
        pCHRBank.fill(0);
        pPRGBank[0] = 0;
        pPRGBank[1] = 0x2000;
        pPRGBank[2] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
        pPRGBank[3] = (uint32_t)(prgBanks * 2 - 1) * 0x2000;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        if (addr < 0xA000) {
            mapped = pPRGBank[0] + (addr & 0x1FFF);
            return true;
        }
        if (addr < 0xC000) {
            mapped = pPRGBank[1] + (addr & 0x1FFF);
            return true;
        }
        if (addr < 0xE000) {
            mapped = pPRGBank[2] + (addr & 0x1FFF);
            return true;
        }
        mapped = pPRGBank[3] + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        if (addr < 0xA000) {
            if ((addr & 1) == 0) {
                targetReg = data & 0x07;
                prgMode = (data & 0x40) != 0;
                chrInversion = (data & 0x80) != 0;
            } else {
                pRegister[targetReg] = data & 0x1F; // tylko 5 bitow danych
                updateBanks();
            }
        }
        // $A000-$FFFF: $A000 ignorowany (mirroring nie stamtad);
        // pozostale rejestry ($C000-$FFFF) wycinamy bo to nie MMC3 z IRQ.
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t slot = (uint8_t)(addr >> 10);
        mapped = pCHRBank[slot] + (addr & 0x03FF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override {
        // Mirroring opisany w nes_specs (zaleznosc od chrInversion) -
        // przyblizenie horizontal/vertical na podstawie wzorca par NTA/NTB.
        uint8_t a = ntA, b = ntB, c = ntC, d = ntD;
        if (a == c && b == d)
            return Mirroring::VERTICAL;
        if (a == b && c == d)
            return Mirroring::HORIZONTAL;
        if (a == b && b == c && c == d)
            return a ? Mirroring::ONESCREEN_HI : Mirroring::ONESCREEN_LO;
        return Mirroring::HORIZONTAL;
    }
    bool hasDynamicMirror() const override { return true; }

private:
    void updateBanks() {
        // CHR (identycznie jak MMC3, ale dane juz zamaskowane do 5 bitow)
        auto r = [&](int i) -> uint32_t {
            return (uint32_t)pRegister[i] * 0x0400;
        };
        if (chrInversion) {
            pCHRBank[0] = r(2);
            pCHRBank[1] = r(3);
            pCHRBank[2] = r(4);
            pCHRBank[3] = r(5);
            pCHRBank[4] = (uint32_t)(pRegister[0] & 0x1E) * 0x0400;
            pCHRBank[5] = pCHRBank[4] + 0x0400;
            pCHRBank[6] = (uint32_t)(pRegister[1] & 0x1E) * 0x0400;
            pCHRBank[7] = pCHRBank[6] + 0x0400;
            // nametables: chrInversion -> [R0,R0,R1,R1] (lub R2..R5 dla
            // 4-screen) Disch: gdy C set -> [R2,R3,R4,R5]
            ntA = (pRegister[2] >> 4) & 1; // bit 5 oryginalnej wartosci -> tu
                                           // bit4 (po >>1?). Doc: "bit 5"
            ntB = (pRegister[3] >> 4) & 1;
            ntC = (pRegister[4] >> 4) & 1;
            ntD = (pRegister[5] >> 4) & 1;
        } else {
            pCHRBank[0] = (uint32_t)(pRegister[0] & 0x1E) * 0x0400;
            pCHRBank[1] = pCHRBank[0] + 0x0400;
            pCHRBank[2] = (uint32_t)(pRegister[1] & 0x1E) * 0x0400;
            pCHRBank[3] = pCHRBank[2] + 0x0400;
            pCHRBank[4] = r(2);
            pCHRBank[5] = r(3);
            pCHRBank[6] = r(4);
            pCHRBank[7] = r(5);
            ntA = (pRegister[0] >> 4) & 1;
            ntB = (pRegister[0] >> 4) & 1;
            ntC = (pRegister[1] >> 4) & 1;
            ntD = (pRegister[1] >> 4) & 1;
        }

        if (prgMode) {
            pPRGBank[0] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
            pPRGBank[2] = (uint32_t)(pRegister[6] & 0x3F) * 0x2000;
        } else {
            pPRGBank[0] = (uint32_t)(pRegister[6] & 0x3F) * 0x2000;
            pPRGBank[2] = (uint32_t)(prgBanks * 2 - 2) * 0x2000;
        }
        pPRGBank[1] = (uint32_t)(pRegister[7] & 0x3F) * 0x2000;
        pPRGBank[3] = (uint32_t)(prgBanks * 2 - 1) * 0x2000;
    }

    std::array<uint8_t, 8> pRegister{};
    std::array<uint32_t, 8> pCHRBank{};
    std::array<uint32_t, 4> pPRGBank{};
    uint8_t targetReg = 0;
    bool prgMode = false;
    bool chrInversion = false;
    uint8_t ntA = 0, ntB = 0, ntC = 0, ntD = 0;
};

REGISTER_MAPPER(95, Mapper095)
