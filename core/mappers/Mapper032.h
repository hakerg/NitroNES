#pragma once
#include "MapperBase.h"
#include "MapperRegistry.h"
#include <array>

// ----------------------------------------------------------------------------
// Mapper 032 - Irem G-101 (Image Fight, Major League, Kaiketsu Yanchamaru 2)
// ----------------------------------------------------------------------------
// PRG: 8 KB switchable @ $8000 (R0), 8 KB switchable @ $A000 (R1), {-2,-1}
// fixed. W trybie 1: R0 mapuje $C000, {-2} fixed @ $8000. CHR: 8x 1 KB
// switchable. $9000: bit0 = PRG mode, bit1 = mirroring (0=V, 1=H).
// ----------------------------------------------------------------------------
class Mapper032 : public Mapper {
public:
    using Mapper::Mapper;

    void reset() override {
        prg0 = 0;
        prg1 = 0;
        prgMode = false;
        mirrorMode = Mirroring::VERTICAL;
        for (auto &c : chr)
            c = 0;
    }

    bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &) override {
        if (addr < 0x8000)
            return false;
        uint8_t lastPrg = (uint8_t)(prgBanks * 2 - 1); // 8K-units
        uint8_t prev = (uint8_t)(prgBanks * 2 - 2);
        uint8_t bank;
        if (addr < 0xA000)
            bank = prgMode ? prev : prg0;
        else if (addr < 0xC000)
            bank = prg1;
        else if (addr < 0xE000)
            bank = prgMode ? prg0 : prev;
        else
            bank = lastPrg;
        mapped = (uint32_t)bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    void cpuMapWrite(uint16_t addr, uint32_t &, uint8_t data) override {
        if (addr < 0x8000)
            return;
        switch (addr & 0xF000) {
        case 0x8000:
            prg0 = data & 0x1F;
            break;
        case 0x9000:
            prgMode = (data & 0x02) != 0;
            mirrorMode =
                (data & 0x01) ? Mirroring::HORIZONTAL : Mirroring::VERTICAL;
            break;
        case 0xA000:
            prg1 = data & 0x1F;
            break;
        case 0xB000:
            chr[addr & 0x07] = data;
            break;
        default:
            break;
        }
    }

    bool ppuMapRead(uint16_t addr, uint32_t &mapped) override {
        if (addr > 0x1FFF)
            return false;
        uint8_t slot = (uint8_t)(addr >> 10);
        mapped = (uint32_t)chr[slot] * 0x0400 + (addr & 0x03FF);
        return true;
    }
    bool ppuMapWrite(uint16_t addr, uint32_t &mapped) override {
        return mapper_helpers::chrRamWrite(addr, mapped, chrBanks);
    }

    Mirroring mirror() const override { return mirrorMode; }
    bool hasDynamicMirror() const override { return true; }

private:
    uint8_t prg0 = 0, prg1 = 0;
    bool prgMode = false;
    Mirroring mirrorMode = Mirroring::VERTICAL;
    std::array<uint8_t, 8> chr{};
};

REGISTER_MAPPER(32, Mapper032)
