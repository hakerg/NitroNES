#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"

// ----------------------------------------------------------------------------
// Mapper 009 - MMC2 / PxROM (Mike Tyson's Punch-Out!!)
// ----------------------------------------------------------------------------
// PRG: 8KB switchable, trzy ko�cowe banki na sta�e (-3, -2, -1).
// CHR: dwie 4KB strefy, ka�da z dw�ch bank�w wybieranych automatycznie przez
// "latche" � ustawiane gdy PPU pobiera kafelek $FD lub $FE.
class Mapper009 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prgBank = 0;
        chrBank0A = chrBank0B = 0;
        chrBank1A = chrBank1B = 0;
        latch0 = latch1 = 0xFE; // zwykle startowo "set"
        mirrorMode = Mirroring::HORIZONTAL;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint32_t total8k = (uint32_t)prgBanks * 2; // banki 8KB
        if (addr < 0xA000) {
            mapped = (uint32_t)(prgBank & 0x0F) * 0x2000 + (addr & 0x1FFF);
        } else if (addr < 0xC000) {
            mapped = (total8k - 3) * 0x2000 + (addr & 0x1FFF);
        } else if (addr < 0xE000) {
            mapped = (total8k - 2) * 0x2000 + (addr & 0x1FFF);
        } else {
            mapped = (total8k - 1) * 0x2000 + (addr & 0x1FFF);
        }
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0xA000)
            return;
        switch (addr & 0xF000) {
        case 0xA000:
            prgBank = data & 0x0F;
            break;
        case 0xB000:
            chrBank0A = data & 0x1F;
            break;
        case 0xC000:
            chrBank0B = data & 0x1F;
            break;
        case 0xD000:
            chrBank1A = data & 0x1F;
            break;
        case 0xE000:
            chrBank1B = data & 0x1F;
            break;
        case 0xF000:
            mirrorMode =
                (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            break;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;

        // Wyb�r banku wed�ug aktualnego stanu latcha
        if (addr < 0x1000) {
            uint8_t bank = (latch0 == 0xFD) ? chrBank0A : chrBank0B;
            mapped = (uint32_t)bank * 0x1000 + (addr & 0x0FFF);
        } else {
            uint8_t bank = (latch1 == 0xFD) ? chrBank1A : chrBank1B;
            mapped = (uint32_t)bank * 0x1000 + (addr & 0x0FFF);
        }

        // Aktualizacja latchy: zachodzi PO odczycie z odpowiedniego adresu.
        // $0FD8 / $0FE8 dla lewej tablicy, $1FD8-DF / $1FE8-EF dla prawej.
        if (addr == 0x0FD8)
            latch0 = 0xFD;
        else if (addr == 0x0FE8)
            latch0 = 0xFE;
        else if (addr >= 0x1FD8 && addr <= 0x1FDF)
            latch1 = 0xFD;
        else if (addr >= 0x1FE8 && addr <= 0x1FEF)
            latch1 = 0xFE;

        return true;
    }

    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prgBank = 0;
    uint8_t chrBank0A = 0, chrBank0B = 0;
    uint8_t chrBank1A = 0, chrBank1B = 0;
    uint8_t latch0 = 0xFE, latch1 = 0xFE;
    Mirroring mirrorMode = Mirroring::HORIZONTAL;
};

REGISTER_MAPPER(9, Mapper009)
